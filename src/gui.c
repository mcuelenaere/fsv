/* gui.c */

/* Higher-level GTK+ interface */

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
#include <gtk/gtk.h>
#include "gui.h"

#include "gnome-color-picker.h"
#include "gnome-dateedit.h"

#include "animation.h"
#include "ogl.h" /* ogl_widget_new( ) */


/* Box packing flags */
enum {
	GUI_PACK_EXPAND	= 1 << 0,
	GUI_PACK_FILL	= 1 << 1,
	GUI_PACK_START	= 1 << 2
};


/* For whenever gtk_main( ) is far away */
void
gui_update( void )
{
	while (gtk_events_pending( ) > 0)
		gtk_main_iteration( );
}


/* This checks if the widget associated with the given adjustment is
 * currently busy redrawing/reconfiguring itself, or is in steady state
 * (this is used when animating widgets to avoid changing the adjustment
 * too often, otherwise the widget can't keep up and things slow down) */
boolean
gui_adjustment_widget_busy( GtkAdjustment *adj )
{
	static const double threshold = (1.0 / 18.0);
	double t_prev;
	double t_now;
	double *tp;

	/* ---- HACK ALERT ----
	 * This doesn't actually check GTK+ internals-- I'm not sure which
	 * ones are relevant here. This just checks the amount of time that
	 * has passed since the last time the function was called with the
	 * same adjustment and returned FALSE, and if it's below a certain
	 * threshold, the object is considered "busy" (returning TRUE) */

	t_now = xgettime( );

	tp = gtk_object_get_data( GTK_OBJECT(adj), "t_prev" );
	if (tp == NULL) {
		tp = NEW(double);
		*tp = t_now;
		gtk_object_set_data_full( GTK_OBJECT(adj), "t_prev", tp, _xfree );
		return FALSE;
	}

	t_prev = *tp;

	if ((t_now - t_prev) > threshold) {
		*tp = t_now;
		return FALSE;
	}

	return TRUE;
}


/* Step/end callback used in animating a GtkAdjustment */
static void
adjustment_step_cb( Morph *morph )
{
	GtkAdjustment *adj;
	double anim_value;

	adj = (GtkAdjustment *)morph->data;
	g_return_if_fail( GTK_IS_ADJUSTMENT(adj) );
	anim_value = *(morph->var);
	if (!gui_adjustment_widget_busy( adj ) || (ABS(morph->end_value - anim_value) < EPSILON))
		gtk_adjustment_set_value( adj, anim_value );
}


/* Creates an integer-valued adjustment */
GtkAdjustment *
gui_int_adjustment( int value, int lower, int upper )
{
	return (GtkAdjustment *)gtk_adjustment_new( (float)value, (float)lower, (float)upper, 1.0, 1.0, 1.0 );
}


/* This places child_w into parent_w intelligently. expand and fill
 * flags are applicable only if parent_w is a box widget */
static void
parent_child_full( GtkWidget *parent_w, GtkWidget *child_w, boolean expand, boolean fill )
{
	bitfield *packing_flags;
	boolean start = TRUE;

	if (parent_w != NULL) {
		if (GTK_IS_BOX(parent_w)) {
			packing_flags = gtk_object_get_data( GTK_OBJECT(parent_w), "packing_flags" );
			if (packing_flags != NULL) {
                                /* Get (non-default) box-packing flags */
				expand = *packing_flags & GUI_PACK_EXPAND;
				fill = *packing_flags & GUI_PACK_FILL;
				start = *packing_flags & GUI_PACK_START;
			}
                        if (start)
				gtk_box_pack_start( GTK_BOX(parent_w), child_w, expand, fill, 0 );
                        else
				gtk_box_pack_end( GTK_BOX(parent_w), child_w, expand, fill, 0 );
		}
		else
			gtk_container_add( GTK_CONTAINER(parent_w), child_w );
		gtk_widget_show( child_w );
	}
}


/* Calls parent_child_full( ) with defaults */
static void
parent_child( GtkWidget *parent_w, GtkWidget *child_w )
{
	parent_child_full( parent_w, child_w, NO_EXPAND, NO_FILL );
}


/* The horizontal box widget */
GtkWidget *
gui_hbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *hbox_w;

	hbox_w = gtk_hbox_new( FALSE, spacing );
	gtk_container_set_border_width( GTK_CONTAINER(hbox_w), spacing );
	parent_child( parent_w, hbox_w );

	return hbox_w;
}


/* The vertical box widget */
GtkWidget *
gui_vbox_add( GtkWidget *parent_w, int spacing )
{
	GtkWidget *vbox_w;

	vbox_w = gtk_vbox_new( FALSE, spacing );
	gtk_container_set_border_width( GTK_CONTAINER(vbox_w), spacing );
	parent_child( parent_w, vbox_w );

	return vbox_w;
}


/* Changes a box widget's default packing flags (i.e. the flags that will
 * be used to pack subsequent children) */
void
gui_box_set_packing( GtkWidget *box_w, boolean expand, boolean fill, boolean start )
{
	static const char data_key[] = "packing_flags";
	bitfield *packing_flags;

	/* Make sure box_w is a box widget */
	g_assert( GTK_IS_BOX(box_w) );
	/* If expand is FALSE, then fill should not be TRUE */
	g_assert( expand || !fill );

	packing_flags = gtk_object_get_data( GTK_OBJECT(box_w), data_key );
	if (packing_flags == NULL) {
		/* Allocate new packing-flags variable for box */
		packing_flags = NEW(bitfield);
		gtk_object_set_data_full( GTK_OBJECT(box_w), data_key, packing_flags, _xfree );
	}

        /* Set flags appropriately */
	*packing_flags = 0;
	*packing_flags |= (expand ? GUI_PACK_EXPAND : 0);
	*packing_flags |= (fill ? GUI_PACK_FILL : 0);
	*packing_flags |= (start ? GUI_PACK_START : 0);
}


/* The standard button widget */
GtkWidget *
gui_button_add( GtkWidget *parent_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *button_w;

	button_w = gtk_button_new( );
	if (label != NULL)
		gui_label_add( button_w, label );
	gtk_signal_connect( GTK_OBJECT(button_w), "clicked", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, button_w );

	return button_w;
}


/* Creates a button with a pixmap prepended to the label */
GtkWidget *
gui_button_with_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *button_w;
	GtkWidget *hbox_w, *hbox2_w;

	button_w = gtk_button_new( );
	parent_child( parent_w, button_w );
	hbox_w = gui_hbox_add( button_w, 0 );
	hbox2_w = gui_hbox_add( hbox_w, 0 );
	gui_widget_packing( hbox2_w, EXPAND, NO_FILL, AT_START );
	gui_pixmap_xpm_add( hbox2_w, xpm_data );
	if (label != NULL) {
		gui_vbox_add( hbox2_w, 2 ); /* spacer */
		gui_label_add( hbox2_w, label );
	}
	gtk_signal_connect( GTK_OBJECT(button_w), "clicked", GTK_SIGNAL_FUNC(callback), callback_data );

	return button_w;
}


/* The toggle button widget */
GtkWidget *
gui_toggle_button_add( GtkWidget *parent_w, const char *label, boolean active, void (*callback)( ), void *callback_data )
{
	GtkWidget *tbutton_w;

	tbutton_w = gtk_toggle_button_new( );
	if (label != NULL)
		gui_label_add( tbutton_w, label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tbutton_w), active );
	gtk_signal_connect( GTK_OBJECT(tbutton_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, tbutton_w );

	return tbutton_w;
}


/* The [multi-column] list widget (fitted into a scrolled window) */
GtkWidget *
gui_clist_add( GtkWidget *parent_w, int num_cols, char *col_titles[] )
{
	GtkWidget *scrollwin_w;
	GtkWidget *clist_w;
	int i;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the clist widget */
	if (col_titles == NULL)
		clist_w = gtk_clist_new( num_cols );
	else
		clist_w = gtk_clist_new_with_titles( num_cols, col_titles );
	gtk_clist_set_selection_mode( GTK_CLIST(clist_w), GTK_SELECTION_SINGLE );
	for (i = 0; i < num_cols; i++)
		gtk_clist_set_column_auto_resize( GTK_CLIST(clist_w), i, TRUE );
	gtk_container_add( GTK_CONTAINER(scrollwin_w), clist_w );
	gtk_widget_show( clist_w );

	return clist_w;
}


/* Scrolls a clist (or ctree) to a given row (-1 indicates last row)
 * WARNING: This implementation does not gracefully handle multiple
 * animated scrolls on the same clist! */
void
gui_clist_moveto_row( GtkWidget *clist_w, int row, double moveto_time )
{
	GtkAdjustment *clist_vadj;
	double *anim_value_var;
	float k, new_value;
	int i;

	if (moveto_time <= 0.0) {
		/* Instant scroll (no animation) */
		if (row >= 0)
			i = row;
		else
			i = GTK_CLIST(clist_w)->rows - 1; /* bottom */
		gtk_clist_moveto( GTK_CLIST(clist_w), i, 0, 0.5, 0.0 );
		return;
	}

	if (row >= 0)
		k = (double)row / (double)GTK_CLIST(clist_w)->rows;
	else
		k = 1.0; /* bottom of clist */
	clist_vadj = gtk_clist_get_vadjustment( GTK_CLIST(clist_w) );
	k = k * clist_vadj->upper - 0.5 * clist_vadj->page_size;
	new_value = CLAMP(k, 0.0, clist_vadj->upper - clist_vadj->page_size);

	/* Allocate an external value variable if clist adjustment doesn't
	 * already have one */
        anim_value_var = gtk_object_get_data( GTK_OBJECT(clist_vadj), "anim_value_var" );
	if (anim_value_var == NULL ); {
		anim_value_var = NEW(double);
		gtk_object_set_data_full( GTK_OBJECT(clist_vadj), "anim_value_var", anim_value_var, _xfree );
	}

	/* If clist is already scrolling, stop it */
	morph_break( anim_value_var );

	/* Begin clist animation */
	*anim_value_var = clist_vadj->value;
	morph_full( anim_value_var, MORPH_SIGMOID, new_value, moveto_time, adjustment_step_cb, adjustment_step_cb, clist_vadj );
}


/* Internal callback for the color picker widget */
static void
color_picker_cb( GtkWidget *colorpicker_w, unsigned int r, unsigned int g, unsigned int b, unsigned int unused, void *data )
{
	void (*user_callback)( RGBcolor *, void * );
	RGBcolor color;

	color.r = (float)r / 65535.0;
	color.g = (float)g / 65535.0;
	color.b = (float)b / 65535.0;

	/* Call user callback */
	user_callback = (void (*)( RGBcolor *, void * ))gtk_object_get_data( GTK_OBJECT(colorpicker_w), "user_callback" );
	(user_callback)( &color, data );
}


/* The color picker widget. Color is initialized to the one given, and the
 * color selection dialog will have the specified title when brought up.
 * Changing the color (i.e. pressing OK in the color selection dialog)
 * activates the given callback */
GtkWidget *
gui_colorpicker_add( GtkWidget *parent_w, RGBcolor *init_color, const char *title, void (*callback)( ), void *callback_data )
{
	GtkWidget *colorpicker_w;

	colorpicker_w = gnome_color_picker_new( );
	gnome_color_picker_set_d( GNOME_COLOR_PICKER(colorpicker_w), init_color->r, init_color->g, init_color->b, 1.0 );
	gnome_color_picker_set_title( GNOME_COLOR_PICKER(colorpicker_w), title );
	gtk_signal_connect( GTK_OBJECT(colorpicker_w), "color_set", GTK_SIGNAL_FUNC(color_picker_cb), callback_data );
	gtk_object_set_data( GTK_OBJECT(colorpicker_w), "user_callback", (void *)callback );
	parent_child( parent_w, colorpicker_w );

	return colorpicker_w;
}


/* Sets the color on a color picker widget */
void
gui_colorpicker_set_color( GtkWidget *colorpicker_w, RGBcolor *color )
{
	gnome_color_picker_set_d( GNOME_COLOR_PICKER(colorpicker_w), color->r, color->g, color->b, 0.0 );
}


/* The tree widget (fitted into a scrolled window) */
GtkWidget *
gui_ctree_add( GtkWidget *parent_w )
{
	GtkWidget *scrollwin_w;
	GtkWidget *ctree_w;

	/* Make the scrolled window widget */
	scrollwin_w = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrollwin_w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        parent_child_full( parent_w, scrollwin_w, EXPAND, FILL );

	/* Make the ctree widget */
	ctree_w = gtk_ctree_new( 1, 0 );
	gtk_clist_set_column_auto_resize( GTK_CLIST(ctree_w), 0, TRUE );
	gtk_ctree_set_indent( GTK_CTREE(ctree_w), 16 );
	gtk_ctree_set_line_style( GTK_CTREE(ctree_w), GTK_CTREE_LINES_DOTTED );
	gtk_clist_set_selection_mode( GTK_CLIST(ctree_w), GTK_SELECTION_BROWSE );
	gtk_ctree_set_spacing( GTK_CTREE(ctree_w), 2 );
	gtk_container_add( GTK_CONTAINER(scrollwin_w), ctree_w );
	gtk_widget_show( ctree_w );

	return ctree_w;
}


/* This adds a new GtkCTreeNode (tree item) to the given ctree.
 * GtkWidget *ctree_w: the ctree widget
 * GtkCTreeNode *parent: the parent node (NULL if creating a top-level node)
 * Icon icon_pair[2]: two icons, for collapsed ([0]) and expanded ([1]) states
 * const char *text: label for node
 * boolean expanded: initial state of node
 * void *data: arbitrary pointer to associate data with node */
GtkCTreeNode *
gui_ctree_node_add( GtkWidget *ctree_w, GtkCTreeNode *parent, Icon icon_pair[2], const char *text, boolean expanded, void *data )
{
	GtkCTreeNode *ctnode;

	ctnode = gtk_ctree_insert_node( GTK_CTREE(ctree_w), parent, NULL, (char **)&text, 1, icon_pair[0].pixmap, icon_pair[0].mask, icon_pair[1].pixmap, icon_pair[1].mask, FALSE, expanded );
	gtk_ctree_node_set_row_data( GTK_CTREE(ctree_w), ctnode, data );

	return ctnode;
}


/* Changes the mouse cursor glyph associated with the given widget.
 * A glyph of -1 indicates the default cursor */
void
gui_cursor( GtkWidget *widget, int glyph )
{
	GdkCursor *prev_cursor, *cursor;
	int *prev_glyph;

	/* Get cursor information from widget */
	prev_cursor = gtk_object_get_data( GTK_OBJECT(widget), "gui_cursor" );
        prev_glyph = gtk_object_get_data( GTK_OBJECT(widget), "gui_glyph" );

	if (prev_glyph == NULL) {
		if (glyph < 0)
			return; /* default cursor is already set */
                /* First-time setup */
		prev_glyph = NEW(int);
		gtk_object_set_data_full( GTK_OBJECT(widget), "gui_glyph", prev_glyph, _xfree );
	}
	else {
		/* Check if requested glyph is same as previous one */
		if (glyph == *prev_glyph)
			return;
	}

	/* Create new cursor and make it active */
	if (glyph >= 0)
		cursor = gdk_cursor_new( (GdkCursorType)glyph );
	else
		cursor = NULL;
	gdk_window_set_cursor( widget->window, cursor );

	/* Don't need the old cursor anymore */
	if (prev_cursor != NULL)
		gdk_cursor_destroy( prev_cursor );

	if (glyph >= 0) {
		/* Save new cursor information */
		gtk_object_set_data( GTK_OBJECT(widget), "gui_cursor", cursor );
		*prev_glyph = glyph;
	}
	else {
		/* Clean up after ourselves */
		gtk_object_remove_data( GTK_OBJECT(widget), "gui_cursor" );
		gtk_object_remove_data( GTK_OBJECT(widget), "gui_glyph" );
	}
}


/* The date edit widget (imported from Gnomeland). The given callback is
 * called whenever the date/time is changed */
GtkWidget *
gui_dateedit_add( GtkWidget *parent_w, time_t the_time, void (*callback)( ), void *callback_data )
{
	GtkWidget *dateedit_w;

	dateedit_w = gnome_date_edit_new( the_time, TRUE, TRUE );
	gnome_date_edit_set_popup_range( GNOME_DATE_EDIT(dateedit_w), 0, 23 );
	gtk_signal_connect( GTK_OBJECT(dateedit_w), "date_changed", GTK_SIGNAL_FUNC(callback), callback_data );
	gtk_signal_connect( GTK_OBJECT(dateedit_w), "time_changed", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, dateedit_w );

	return dateedit_w;
}


/* Reads current time from a date edit widget */
time_t
gui_dateedit_get_time( GtkWidget *dateedit_w )
{
	return gnome_date_edit_get_date( GNOME_DATE_EDIT(dateedit_w) );
}


/* Sets the time on a date edit widget */
void
gui_dateedit_set_time( GtkWidget *dateedit_w, time_t the_time )
{
	gnome_date_edit_set_time( GNOME_DATE_EDIT(dateedit_w), the_time );
}


/* The entry (text input) widget */
GtkWidget *
gui_entry_add( GtkWidget *parent_w, const char *init_text, void (*callback)( ), void *callback_data )
{
	GtkWidget *entry_w;

	entry_w = gtk_entry_new( );
        if (init_text != NULL)
		gtk_entry_set_text( GTK_ENTRY(entry_w), init_text );
	if (callback != NULL )
		gtk_signal_connect( GTK_OBJECT(entry_w), "activate", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child_full( parent_w, entry_w, EXPAND, FILL );

	return entry_w;
}


/* Sets the text in an entry to the specified string */
void
gui_entry_set_text( GtkWidget *entry_w, const char *entry_text )
{
	gtk_entry_set_text( GTK_ENTRY(entry_w), entry_text );
}


/* Returns the text currently in an entry */
char *
gui_entry_get_text( GtkWidget *entry_w )
{
	return gtk_entry_get_text( GTK_ENTRY(entry_w) );
}


/* Highlights the text in an entry */
void
gui_entry_highlight( GtkWidget *entry_w )
{
	gtk_entry_select_region( GTK_ENTRY(entry_w), 0, GTK_ENTRY(entry_w)->text_length );
}


/* The frame widget (with optional title) */
GtkWidget *
gui_frame_add( GtkWidget *parent_w, const char *title )
{
	GtkWidget *frame_w;

	frame_w = gtk_frame_new( title );
	parent_child_full( parent_w, frame_w, EXPAND, FILL );

	return frame_w;
}


/* The OpenGL area widget */
GtkWidget *
gui_gl_area_add( GtkWidget *parent_w )
{
	GtkWidget *gl_area_w;
	int bitmask = 0;

	gl_area_w = ogl_widget_new( );
	bitmask |= GDK_EXPOSURE_MASK;
	bitmask |= GDK_POINTER_MOTION_MASK;
	bitmask |= GDK_BUTTON_MOTION_MASK;
	bitmask |= GDK_BUTTON1_MOTION_MASK;
	bitmask |= GDK_BUTTON2_MOTION_MASK;
	bitmask |= GDK_BUTTON3_MOTION_MASK;
	bitmask |= GDK_BUTTON_PRESS_MASK;
	bitmask |= GDK_BUTTON_RELEASE_MASK;
	bitmask |= GDK_LEAVE_NOTIFY_MASK;
	gtk_widget_set_events( GTK_WIDGET(gl_area_w), bitmask );
	parent_child_full( parent_w, gl_area_w, EXPAND, FILL );

	return gl_area_w;
}


/* Sets up keybindings (accelerators). Call this any number of times with
 * widget/keystroke pairs, and when all have been specified, call with the
 * parent window widget (and no keystroke) to attach the keybindings.
 * Keystroke syntax: "K" == K keypress, "^K" == Ctrl-K */
void
gui_keybind( GtkWidget *widget, char *keystroke )
{
	static GtkAccelGroup *accel_group = NULL;
	int mods;
	char key;

	if (accel_group == NULL)
		accel_group = gtk_accel_group_new( );

	if (GTK_IS_WINDOW(widget)) {
		/* Attach keybindings */
		gtk_accel_group_attach( accel_group, GTK_OBJECT(widget) );
		accel_group = NULL;
		return;
	}

	/* Parse keystroke string */
	switch (keystroke[0]) {
		case '^':
		/* Ctrl-something keystroke specified */
		mods = GDK_CONTROL_MASK;
		key = keystroke[1];
		break;

		default:
		/* Simple keypress */
		mods = 0;
		key = keystroke[0];
		break;
	}

	if (GTK_IS_MENU_ITEM(widget)) {
		gtk_widget_add_accelerator( widget, "activate", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}
	if (GTK_IS_BUTTON(widget)) {
		gtk_widget_add_accelerator( widget, "clicked", accel_group, key, mods, GTK_ACCEL_VISIBLE );
		return;
	}

	/* Make widget grab focus when its key is pressed */
	gtk_widget_add_accelerator( widget, "grab_focus", accel_group, key, mods, GTK_ACCEL_VISIBLE );
}


/* The label widget */
GtkWidget *
gui_label_add( GtkWidget *parent_w, const char *label_text )
{
	GtkWidget *label_w;
	GtkWidget *hbox_w;

	label_w = gtk_label_new( label_text );
	if (parent_w != NULL) {
		if (GTK_IS_BUTTON(parent_w)) {
			/* Labels are often too snug inside buttons */
			hbox_w = gui_hbox_add( parent_w, 0 );
			gtk_box_pack_start( GTK_BOX(hbox_w), label_w, TRUE, FALSE, 5 );
			gtk_widget_show( label_w );
		}
		else
			parent_child( parent_w, label_w );
	}

	return label_w;
}


/* Adds a menu to a menu bar, or a submenu to a menu */
GtkWidget *
gui_menu_add( GtkWidget *parent_menu_w, const char *label )
{
	GtkWidget *menu_item_w;
	GtkWidget *menu_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	/* parent_menu can be a menu bar or a regular menu */
	if (GTK_IS_MENU_BAR(parent_menu_w))
		gtk_menu_bar_append( GTK_MENU_BAR(parent_menu_w), menu_item_w );
	else
		gtk_menu_append( GTK_MENU(parent_menu_w), menu_item_w );
	gtk_widget_show( menu_item_w );
	menu_w = gtk_menu_new( );
	gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu_item_w), menu_w );
	/* Bug in GTK+? Following pointer shouldn't be NULL */
	GTK_MENU(menu_w)->parent_menu_item = menu_item_w;

	return menu_w;
}


/* Adds a menu item to a menu */
GtkWidget *
gui_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *menu_item_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	gtk_menu_append( GTK_MENU(menu_w), menu_item_w );
	if (callback != NULL)
		gtk_signal_connect( GTK_OBJECT(menu_item_w), "activate", GTK_SIGNAL_FUNC(callback), callback_data );
	gtk_widget_show( menu_item_w );

	return menu_item_w;
}


/* This initiates the definition of a radio menu item group. The item in
 * the specified position will be the one that is initially selected
 * (0 == first, 1 == second, and so on) */
void
gui_radio_menu_begin( int init_selected )
{
	gui_radio_menu_item_add( NULL, NULL, NULL, &init_selected );
}


/* Adds a radio menu item to a menu. Don't forget to call
 * gui_radio_menu_begin( ) first.
 * WARNING: When the initially selected menu item is set, the first item
 * in the group will be "toggled" off. The callback should either watch
 * for this, or do nothing if the widget's "active" flag is FALSE */
GtkWidget *
gui_radio_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data )
{
	static GSList *radio_group;
	static int init_selected;
	static int radmenu_item_num;
	GtkWidget *radmenu_item_w = NULL;

	if (menu_w == NULL) {
		/* We're being called from begin_radio_menu_group( ) */
		radio_group = NULL;
		radmenu_item_num = 0;
		init_selected = *((int *)callback_data);
	}
	else {
		radmenu_item_w = gtk_radio_menu_item_new_with_label( radio_group, label );
		radio_group = gtk_radio_menu_item_group( GTK_RADIO_MENU_ITEM(radmenu_item_w) );
		gtk_menu_append( GTK_MENU(menu_w), radmenu_item_w );
		if (radmenu_item_num == init_selected)
			gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(radmenu_item_w), TRUE );
		gtk_signal_connect( GTK_OBJECT(radmenu_item_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
		gtk_widget_show( radmenu_item_w );
		++radmenu_item_num;
	}

	return radmenu_item_w;
}


/* The option menu widget. Options must have already been defined using
 * gui_option_menu_item( ) */
GtkWidget *
gui_option_menu_add( GtkWidget *parent_w, int init_selected )
{
	static GtkWidget *menu_w = NULL;
	GtkWidget *optmenu_w = NULL;

	if (GTK_IS_MENU_ITEM(parent_w)) {
		/* gui_option_menu_item( ) has a menu item for us */
		if (menu_w == NULL)
			menu_w = gtk_menu_new( );
		gtk_menu_append( GTK_MENU(menu_w), parent_w );
		gtk_widget_show( parent_w );
	}
	else {
		/* Make the finished option menu */
		optmenu_w = gtk_option_menu_new( );
		gtk_option_menu_set_menu( GTK_OPTION_MENU(optmenu_w), menu_w );
		gtk_option_menu_set_history( GTK_OPTION_MENU(optmenu_w), init_selected );
		parent_child( parent_w, optmenu_w );
		menu_w = NULL;
	}

	return optmenu_w;
}


/* Option menu definiton. Call this once for each menu item, and then call
 * gui_option_menu_add( ) to produce the finished widget */
GtkWidget *
gui_option_menu_item( const char *label, void (*callback)( ), void *callback_data )
{
	GtkWidget *menu_item_w;

	menu_item_w = gtk_menu_item_new_with_label( label );
	if (callback != NULL)
		gtk_signal_connect( GTK_OBJECT(menu_item_w), "activate", GTK_SIGNAL_FUNC(callback), callback_data );
	gui_option_menu_add( menu_item_w, NIL );

	return menu_item_w;
}


/* The notebook widget */
GtkWidget *
gui_notebook_add( GtkWidget *parent_w )
{
	GtkWidget *notebook_w;

	notebook_w = gtk_notebook_new( );
	parent_child_full( parent_w, notebook_w, EXPAND, FILL );

	return notebook_w;
}


/* Adds a new page to a notebook, with the given tab label, and whose
 * content is defined by the given widget */
void
gui_notebook_page_add( GtkWidget *notebook_w, const char *tab_label, GtkWidget *content_w )
{
	GtkWidget *tab_label_w;

	tab_label_w = gtk_label_new( tab_label );
	gtk_notebook_append_page( GTK_NOTEBOOK(notebook_w), content_w, tab_label_w );
	gtk_widget_show( tab_label_w );
	gtk_widget_show( content_w );
}


/* Horizontal paned window widget */
GtkWidget *
gui_hpaned_add( GtkWidget *parent_w, int divider_x_pos )
{
	GtkWidget *hpaned_w;

	hpaned_w = gtk_hpaned_new( );
	gtk_paned_set_position( GTK_PANED(hpaned_w), divider_x_pos );
	parent_child_full( parent_w, hpaned_w, EXPAND, FILL );

	return hpaned_w;
}


/* Vertical paned window widget */
GtkWidget *
gui_vpaned_add( GtkWidget *parent_w, int divider_y_pos )
{
	GtkWidget *vpaned_w;

	vpaned_w = gtk_vpaned_new( );
	gtk_paned_set_position( GTK_PANED(vpaned_w), divider_y_pos );
	parent_child_full( parent_w, vpaned_w, EXPAND, FILL );

	return vpaned_w;
}


/* The pixmap widget (created from XPM data) */
GtkWidget *
gui_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data )
{
	GtkWidget *pixmap_w;
	GtkStyle *style;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	/* Realize parent widget to prevent "NULL window" error */
	gtk_widget_realize( parent_w );
	style = gtk_widget_get_style( parent_w );
	pixmap = gdk_pixmap_create_from_xpm_d( parent_w->window, &mask, &style->bg[GTK_STATE_NORMAL], xpm_data );
	pixmap_w = gtk_pixmap_new( pixmap, mask );
	gdk_pixmap_unref( pixmap );
	gdk_bitmap_unref( mask );
	parent_child( parent_w, pixmap_w );

	return pixmap_w;
}


/* The color preview widget */
GtkWidget *
gui_preview_add( GtkWidget *parent_w )
{
	GtkWidget *preview_w;

	preview_w = gtk_preview_new( GTK_PREVIEW_COLOR );
	parent_child_full( parent_w, preview_w, EXPAND, FILL );

	return preview_w;
}


/* Helper callback for gui_preview_spectrum( ) */
/* BUG: This does not handle resizes correctly */
static int
preview_spectrum_draw_cb( GtkWidget *preview_w, void *unused, const char *evtype )
{
	RGBcolor (*spectrum_func)( double x );
	RGBcolor color;
	int width, height;
	int prev_width, prev_height;
	int i;
	unsigned char *rowbuf;

	width = preview_w->allocation.width;
	height = preview_w->allocation.height;

	prev_width = GTK_PREVIEW(preview_w)->buffer_width;
	prev_height = GTK_PREVIEW(preview_w)->buffer_height;

	/* Set new preview size if allocation has changed */
	if ((width != prev_width) || (height != prev_height))
		gtk_preview_size( GTK_PREVIEW(preview_w), width, height );
	else if (!strcmp( evtype, "expose" ))
		return FALSE;

	if (!GTK_WIDGET_DRAWABLE(preview_w))
		return FALSE;

	/* Get spectrum function */
	spectrum_func = (RGBcolor (*)( double x ))gtk_object_get_data( GTK_OBJECT(preview_w), "spectrum_func" );

	/* Create one row of the spectrum image */
	rowbuf = NEW_ARRAY(unsigned char, 3 * width);
	for (i = 0; i < width; i++) {
		color = (spectrum_func)( (double)i / (double)(width - 1) ); /* struct assign */
		rowbuf[3 * i] = (unsigned char)(255.0 * color.r);
		rowbuf[3 * i + 1] = (unsigned char)(255.0 * color.g);
		rowbuf[3 * i + 2] = (unsigned char)(255.0 * color.b);
	}

	/* Draw spectrum into preview widget, row by row */
	for (i = 0; i < height; i++)
		gtk_preview_draw_row( GTK_PREVIEW(preview_w), rowbuf, 0, i, width );
	xfree( rowbuf );

	gtk_widget_draw( preview_w, NULL );

	return FALSE;
}


/* Fills a preview widget with an arbitrary spectrum. Second argument
 * should be a function returning the appropriate color at a specified
 * fractional position in the spectrum */
void
gui_preview_spectrum( GtkWidget *preview_w, RGBcolor (*spectrum_func)( double x ) )
{
	static const char data_key[] = "spectrum_func";
	boolean first_time;

        /* Check if this is first-time initialization */
        first_time = gtk_object_get_data( GTK_OBJECT(preview_w), data_key ) == NULL;

	/* Attach spectrum function to preview widget */
	gtk_object_set_data( GTK_OBJECT(preview_w), data_key, (void *)spectrum_func );

	if (first_time) {
		/* Attach draw callback */
		gtk_signal_connect( GTK_OBJECT(preview_w), "expose_event", GTK_SIGNAL_FUNC(preview_spectrum_draw_cb), "expose" );
		gtk_signal_connect( GTK_OBJECT(preview_w), "size_allocate", GTK_SIGNAL_FUNC(preview_spectrum_draw_cb), "size" );
	}
	else
		preview_spectrum_draw_cb( preview_w, NULL, "redraw" );
}


/* The horizontal scrollbar widget */
GtkWidget *
gui_hscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *hscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	hscrollbar_w = gtk_hscrollbar_new( adjustment );
	gtk_container_add( GTK_CONTAINER(frame_w), hscrollbar_w );
	gtk_widget_show( hscrollbar_w );

	return hscrollbar_w;
}


/* The vertical scrollbar widget */
GtkWidget *
gui_vscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment )
{
	GtkWidget *frame_w;
	GtkWidget *vscrollbar_w;

	/* Make a nice-looking frame to put the scrollbar in */
	frame_w = gui_frame_add( NULL, NULL );
	parent_child( parent_w, frame_w );

	vscrollbar_w = gtk_vscrollbar_new( adjustment );
	gtk_container_add( GTK_CONTAINER(frame_w), vscrollbar_w );
	gtk_widget_show( vscrollbar_w );

	return vscrollbar_w;
}


/* The (ever-ubiquitous) separator widget */
GtkWidget *
gui_separator_add( GtkWidget *parent_w )
{
	GtkWidget *separator_w;

	if (parent_w != NULL) {
		if (GTK_IS_MENU(parent_w)) {
			separator_w = gtk_menu_item_new( );
			gtk_menu_append( GTK_MENU(parent_w), separator_w );
		}
		else {
			separator_w = gtk_hseparator_new( );
			gtk_box_pack_start( GTK_BOX(parent_w), separator_w, FALSE, FALSE, 10 );
		}
		gtk_widget_show( separator_w );
	}
	else
		separator_w = gtk_hseparator_new( );

	return separator_w;
}


/* The statusbar widget */
GtkWidget *
gui_statusbar_add( GtkWidget *parent_w )
{
	GtkWidget *statusbar_w;

	statusbar_w = gtk_statusbar_new( );
	parent_child( parent_w, statusbar_w );

	return statusbar_w;
}


/* Displays the given message in the given statusbar widget */
void
gui_statusbar_message( GtkWidget *statusbar_w, const char *message )
{
	char strbuf[1024];

	if (GTK_STATUSBAR(statusbar_w)->messages == NULL)
		gtk_statusbar_push( GTK_STATUSBAR(statusbar_w), 1, "" );

	gtk_statusbar_pop( GTK_STATUSBAR(statusbar_w), 1 );
	/* Prefix a space so that text doesn't touch left edge */
	snprintf( strbuf, sizeof(strbuf), " %s", message );
	gtk_statusbar_push( GTK_STATUSBAR(statusbar_w), 1, strbuf );
}


/* The table (layout) widget */
GtkWidget *
gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding )
{
	GtkWidget *table_w;
	int *cp;

	table_w = gtk_table_new( num_rows, num_cols, homog );
	cp = NEW(int);
	*cp = cell_padding;
        gtk_object_set_data_full( GTK_OBJECT(table_w), "cell_padding", cp, _xfree );
	parent_child_full( parent_w, table_w, EXPAND, FILL );

	return table_w;
}


/* Attaches a widget to a table */
void
gui_table_attach( GtkWidget *table_w, GtkWidget *widget, int left, int right, int top, int bottom )
{
	int cp;

	cp = *(int *)gtk_object_get_data( GTK_OBJECT(table_w), "cell_padding" );
	gtk_table_attach( GTK_TABLE(table_w), widget, left, right, top, bottom, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, cp, cp );
	gtk_widget_show( widget );
}


/* The text (area) widget, optionally initialized with text */
GtkWidget *
gui_text_area_add( GtkWidget *parent_w, const char *init_text )
{
	GtkWidget *text_area_w;

	/* Text (area) widget */
	text_area_w = gtk_text_new( NULL, NULL );
	gtk_text_set_editable( GTK_TEXT(text_area_w), FALSE );
	gtk_text_set_word_wrap( GTK_TEXT(text_area_w), TRUE );
	if (init_text != NULL)
		gtk_text_insert( GTK_TEXT(text_area_w), NULL, NULL, NULL, init_text, -1 );
	parent_child( parent_w, text_area_w );

	return text_area_w;
}


/* This changes the packing flags of a widget inside a box widget. This
 * allows finer control than gtk_box_set_packing( ) (as this only affects
 * a single widget) */
void
gui_widget_packing( GtkWidget *widget, boolean expand, boolean fill, boolean start )
{
	GtkWidget *parent_box_w;

	parent_box_w = widget->parent;
	g_assert( GTK_IS_BOX(parent_box_w) );

	gtk_box_set_child_packing( GTK_BOX(parent_box_w), widget, expand, fill, 0, start ? GTK_PACK_START : GTK_PACK_END );
}


/* Internal callback for the color selection window, called when the
 * OK button is pressed */
static void
colorsel_window_cb( GtkWidget *colorsel_window_w )
{
        RGBcolor color;
	double color_rgb[4];
	void (*user_callback)( const RGBcolor *, void * );
	void *user_callback_data;

	gtk_color_selection_get_color( GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(colorsel_window_w)->colorsel), color_rgb );
	color.r = color_rgb[0];
	color.g = color_rgb[1];
	color.b = color_rgb[2];

	user_callback = (void (*)( const RGBcolor *, void * ))gtk_object_get_data( GTK_OBJECT(colorsel_window_w), "user_callback" );
	user_callback_data = gtk_object_get_data( GTK_OBJECT(colorsel_window_w), "user_callback_data" );
	gtk_widget_destroy( colorsel_window_w );

	/* Call user callback */
	(user_callback)( &color, user_callback_data );
}


/* Creates a color selection window. OK button activates ok_callback */
GtkWidget *
gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *colorsel_window_w;
	GtkColorSelectionDialog *csd;
	double color_rgb[3];

	colorsel_window_w = gtk_color_selection_dialog_new( title );
	csd = GTK_COLOR_SELECTION_DIALOG(colorsel_window_w);
	color_rgb[0] = init_color->r;
	color_rgb[1] = init_color->g;
	color_rgb[2] = init_color->b;
	gtk_color_selection_set_color( GTK_COLOR_SELECTION(csd->colorsel), color_rgb );
	gtk_widget_hide( csd->help_button );
	gtk_object_set_data( GTK_OBJECT(colorsel_window_w), "user_callback", (void *)ok_callback );
	gtk_object_set_data( GTK_OBJECT(colorsel_window_w), "user_callback_data", ok_callback_data );
	gtk_signal_connect_object( GTK_OBJECT(csd->ok_button), "clicked", GTK_SIGNAL_FUNC(colorsel_window_cb), GTK_OBJECT(colorsel_window_w) );
	gtk_signal_connect_object( GTK_OBJECT(csd->cancel_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(colorsel_window_w) );
	gtk_widget_show( colorsel_window_w );

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(colorsel_window_w), TRUE );

	return colorsel_window_w;
}


/* Creates a base dialog window. close_callback is called when the
 * window is destroyed */
GtkWidget *
gui_dialog_window( const char *title, void (*close_callback)( ) )
{
	GtkWidget *window_w;

	window_w = gtk_window_new( GTK_WINDOW_DIALOG );
	gtk_window_set_policy( GTK_WINDOW(window_w), FALSE, FALSE, FALSE );
	gtk_window_set_position( GTK_WINDOW(window_w), GTK_WIN_POS_CENTER );
	gtk_window_set_title( GTK_WINDOW(window_w), title );
	gtk_signal_connect( GTK_OBJECT(window_w), "delete_event", GTK_SIGNAL_FUNC(gtk_widget_destroy), NULL );
	if (close_callback != NULL)
		gtk_signal_connect( GTK_OBJECT(window_w), "destroy", GTK_SIGNAL_FUNC(close_callback), NULL );
	/* !gtk_widget_show( ) */

	return window_w;
}


/* Internal callback for the text-entry window, called when the
 * OK button is pressed */
static void
entry_window_cb( GtkWidget *unused, GtkWidget *entry_window_w )
{
	GtkWidget *entry_w;
	char *entry_text;
	void (*user_callback)( const char *, void * );
	void *user_callback_data;

	entry_w = gtk_object_get_data( GTK_OBJECT(entry_window_w), "entry_w" );
	entry_text = xstrdup( gtk_entry_get_text( GTK_ENTRY(entry_w) ) );

	user_callback = (void (*)( const char *, void * ))gtk_object_get_data( GTK_OBJECT(entry_window_w), "user_callback" );
	user_callback_data = gtk_object_get_data( GTK_OBJECT(entry_window_w), "user_callback_data" );
	gtk_widget_destroy( entry_window_w );

	/* Call user callback */
	(user_callback)( entry_text, user_callback_data );
        xfree( entry_text );
}


/* Creates a one-line text-entry window, initialized with the given text
 * string. OK button activates ok_callback */
GtkWidget *
gui_entry_window( const char *title, const char *init_text, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *entry_window_w;
	GtkWidget *frame_w;
	GtkWidget *vbox_w;
	GtkWidget *entry_w;
	GtkWidget *hbox_w;
	GtkWidget *button_w;
        int width;

	entry_window_w = gui_dialog_window( title, NULL );
	gtk_container_set_border_width( GTK_CONTAINER(entry_window_w), 5 );
	width = gdk_screen_width( ) / 2;
	gtk_widget_set_usize( entry_window_w, width, 0 );
	gtk_object_set_data( GTK_OBJECT(entry_window_w), "user_callback", (void *)ok_callback );
	gtk_object_set_data( GTK_OBJECT(entry_window_w), "user_callback_data", ok_callback_data );

	frame_w = gui_frame_add( entry_window_w, NULL );
	vbox_w = gui_vbox_add( frame_w, 10 );

        /* Text entry widget */
	entry_w = gui_entry_add( vbox_w, init_text, entry_window_cb, entry_window_w );
	gtk_object_set_data( GTK_OBJECT(entry_window_w), "entry_w", entry_w );

	/* Horizontal box for buttons */
	hbox_w = gui_hbox_add( vbox_w, 0 );
	gtk_box_set_homogeneous( GTK_BOX(hbox_w), TRUE );
	gui_box_set_packing( hbox_w, EXPAND, FILL, AT_START );

	/* OK/Cancel buttons */
	gui_button_add( hbox_w, _("OK"), entry_window_cb, entry_window_w );
	vbox_w = gui_vbox_add( hbox_w, 0 ); /* spacer */
	button_w = gui_button_add( hbox_w, _("Cancel"), NULL, NULL );
	gtk_signal_connect_object( GTK_OBJECT(button_w), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(entry_window_w) );

	gtk_widget_show( entry_window_w );
	gtk_widget_grab_focus( entry_w );

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(entry_window_w), TRUE );

	return entry_window_w;
}


/* Internal callback for the file selection window, called when the
 * OK button is pressed */
static void
filesel_window_cb( GtkWidget *filesel_w )
{
	char *filename;
	void (*user_callback)( const char *, void * );
        void *user_callback_data;

	filename = xstrdup( gtk_file_selection_get_filename( GTK_FILE_SELECTION(filesel_w) ) );
	user_callback = (void (*)( const char *, void * ))gtk_object_get_data( GTK_OBJECT(filesel_w), "user_callback" );
	user_callback_data = gtk_object_get_data( GTK_OBJECT(filesel_w), "user_callback_data" );
	gtk_widget_destroy( filesel_w );

	/* Call user callback */
	(user_callback)( filename, user_callback_data );

	xfree( filename );
}


/* Creates a file selection window, with an optional default filename.
 * OK button activates ok_callback */
GtkWidget *
gui_filesel_window( const char *title, const char *init_filename, void (*ok_callback)( ), void *ok_callback_data )
{
	GtkWidget *filesel_window_w;

	filesel_window_w = gtk_file_selection_new( title );
	if (init_filename != NULL)
		gtk_file_selection_set_filename( GTK_FILE_SELECTION(filesel_window_w), init_filename );
	gtk_window_set_position( GTK_WINDOW(filesel_window_w), GTK_WIN_POS_CENTER );
	gtk_object_set_data( GTK_OBJECT(filesel_window_w), "user_callback", (void *)ok_callback );
	gtk_object_set_data( GTK_OBJECT(filesel_window_w), "user_callback_data", ok_callback_data );
	gtk_signal_connect_object( GTK_OBJECT(GTK_FILE_SELECTION(filesel_window_w)->ok_button), "clicked", GTK_SIGNAL_FUNC(filesel_window_cb), GTK_OBJECT(filesel_window_w) );
	gtk_signal_connect_object( GTK_OBJECT(GTK_FILE_SELECTION(filesel_window_w)->cancel_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(filesel_window_w) );
	gtk_signal_connect_object( GTK_OBJECT(GTK_FILE_SELECTION(filesel_window_w)->cancel_button), "delete_event", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(filesel_window_w) );
        /* no gtk_widget_show( ) */

	if (gtk_grab_get_current( ) != NULL)
		gtk_window_set_modal( GTK_WINDOW(filesel_window_w), TRUE );

	return filesel_window_w;
}


/* Associates an icon (created from XPM data) to a window */
void
gui_window_icon_xpm( GtkWidget *window_w, char **xpm_data )
{
	GtkStyle *style;
	GdkPixmap *icon_pixmap;
	GdkBitmap *mask;

	gtk_widget_realize( window_w );
	style = gtk_widget_get_style( window_w );
	icon_pixmap = gdk_pixmap_create_from_xpm_d( window_w->window, &mask, &style->bg[GTK_STATE_NORMAL], xpm_data );
	gdk_window_set_icon( window_w->window, NULL, icon_pixmap, mask );
}


/* Helper function for gui_window_modalize( ), called upon the destruction
 * of the modal window */
static void
window_unmodalize( GtkObject *unused, GtkWidget *parent_window_w )
{
	gtk_widget_set_sensitive( parent_window_w, TRUE );
	gui_cursor( parent_window_w, -1 );
}


/* Makes a window modal w.r.t its parent window */
void
gui_window_modalize( GtkWidget *window_w, GtkWidget *parent_window_w )
{
	gtk_window_set_transient_for( GTK_WINDOW(window_w), GTK_WINDOW(parent_window_w) );
	gtk_window_set_modal( GTK_WINDOW(window_w), TRUE );
	gtk_widget_set_sensitive( parent_window_w, FALSE );
	gui_cursor( parent_window_w, GDK_X_CURSOR );

	/* Restore original state once the window is destroyed */
	gtk_signal_connect( GTK_OBJECT(window_w), "destroy", GTK_SIGNAL_FUNC(window_unmodalize), parent_window_w );
}


#if 0
/* The following is stuff that isn't being used right now (obviously),
 * but may be in the future. TODO: Delete this section by v1.0! */


/* The check button widget */
GtkWidget *
gui_check_button_add( GtkWidget *parent_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *cbutton_w;

	cbutton_w = gtk_check_button_new_with_label( label );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(cbutton_w), init_state );
	gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(cbutton_w), TRUE );
	if (callback != NULL)
		gtk_signal_connect( GTK_OBJECT(cbutton_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
	parent_child( parent_w, cbutton_w );

	return cbutton_w;
}


/* Adds a check menu item to a menu */
GtkWidget *
gui_check_menu_item_add( GtkWidget *menu_w, const char *label, boolean init_state, void (*callback)( ), void *callback_data )
{
	GtkWidget *chkmenu_item_w;

	chkmenu_item_w = gtk_check_menu_item_new_with_label( label );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(chkmenu_item_w), init_state );
	gtk_check_menu_item_set_show_toggle( GTK_CHECK_MENU_ITEM(chkmenu_item_w), TRUE );
	gtk_menu_append( GTK_MENU(menu_w), chkmenu_item_w );
	gtk_signal_connect( GTK_OBJECT(chkmenu_item_w), "toggled", GTK_SIGNAL_FUNC(callback), callback_data );
	gtk_widget_show( chkmenu_item_w );

	return chkmenu_item_w;
}


/* Resizes an entry to fit the width of the specified string */
void
gui_entry_set_width( GtkWidget *entry_w, const char *str )
{
	GtkStyle *style;
	int width;

	style = gtk_widget_get_style( entry_w );
	width = gdk_string_width( style->font, str );
	gtk_widget_set_usize( entry_w, width + 16, 0 );
}


/* The spin button widget */
GtkWidget *
gui_spin_button_add( GtkWidget *parent_w, GtkAdjustment *adj )
{
	GtkWidget *spinbtn_w;

	spinbtn_w = gtk_spin_button_new( adj, 0.0, 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), spinbtn_w, FALSE, FALSE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), spinbtn_w );
	gtk_widget_show( spinbtn_w );

	return spinbtn_w;
}


/* Returns the width of string, when drawn in the given widget */
int
gui_string_width( const char *str, GtkWidget *widget )
{
	GtkStyle *style;
	style = gtk_widget_get_style( widget );
	return gdk_string_width( style->font, str );
}


/* The horizontal value slider widget */
GtkWidget *
gui_hscale_add( GtkWidget *parent_w, GtkObject *adjustment )
{
	GtkWidget *hscale_w;

	hscale_w = gtk_hscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_digits( GTK_SCALE(hscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), hscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), hscale_w );
	gtk_widget_show( hscale_w );

	return hscale_w;
}


/* The vertical value slider widget */
GtkWidget *
gui_vscale_add( GtkWidget *parent_w, GtkObject *adjustment )
{
	GtkWidget *vscale_w;

	vscale_w = gtk_vscale_new( GTK_ADJUSTMENT(adjustment) );
	gtk_scale_set_value_pos( GTK_SCALE(vscale_w), GTK_POS_RIGHT );
	gtk_scale_set_digits( GTK_SCALE(vscale_w), 0 );
	if (GTK_IS_BOX(parent_w))
		gtk_box_pack_start( GTK_BOX(parent_w), vscale_w, TRUE, TRUE, 0 );
	else
		gtk_container_add( GTK_CONTAINER(parent_w), vscale_w );
	gtk_widget_show( vscale_w );

	return vscale_w;
}


/* Associates a tooltip with a widget */
void
gui_tooltip_add( GtkWidget *widget, const char *tip_text )
{
	static GtkTooltips *tooltips = NULL;

	if (tooltips == NULL) {
		tooltips = gtk_tooltips_new( );
		gtk_tooltips_set_delay( tooltips, 2000 );
	}
	gtk_tooltips_set_tip( tooltips, widget, tip_text, NULL );
}
#endif /* 0 */


/* end gui.c */
