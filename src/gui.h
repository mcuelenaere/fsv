/* gui.h */

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


#ifdef FSV_GUI_H
	#error
#endif
#define FSV_GUI_H


#ifdef __GTK_H__

/* For clearer gui_*_packing( ) syntax */
#define EXPAND		TRUE
#define NO_EXPAND	FALSE
#define FILL		TRUE
#define NO_FILL		FALSE
#define AT_START	TRUE
#define AT_END		FALSE


/* Icon container */
typedef struct _Icon Icon;
struct _Icon {
	GdkPixmap *pixmap;
	GdkBitmap *mask;
};

#endif /* __GTK_H__ */


void gui_update( void );
#ifdef __GTK_H__
boolean gui_adjustment_widget_busy( GtkAdjustment *adj );
GtkAdjustment *gui_int_adjustment( int value, int lower, int upper );
GtkWidget *gui_hbox_add( GtkWidget *parent_w, int spacing );
GtkWidget *gui_vbox_add( GtkWidget *parent_w, int spacing );
void gui_box_set_packing( GtkWidget *box_w, boolean expand, boolean fill, boolean start );
GtkWidget *gui_button_add( GtkWidget *parent_w, const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_button_with_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data, const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_toggle_button_add( GtkWidget *parent_w, const char *label, boolean active, void (*callback)( ), void *callback_data );
GtkWidget *gui_clist_add( GtkWidget *parent_w, int num_cols, char *col_titles[] );
void gui_clist_moveto_row( GtkWidget *clist_w, int row, double moveto_time );
GtkWidget *gui_colorpicker_add( GtkWidget *parent_w, RGBcolor *init_color, const char *title, void (*callback)( ), void *callback_data );
void gui_colorpicker_set_color( GtkWidget *colorpicker_w, RGBcolor *color );
GtkWidget *gui_ctree_add( GtkWidget *parent_w );
GtkCTreeNode *gui_ctree_node_add( GtkWidget *ctree_w, GtkCTreeNode *parent, Icon icon_pair[2], const char *text, boolean expanded, void *data );
void gui_cursor( GtkWidget *widget, int glyph );
GtkWidget *gui_dateedit_add( GtkWidget *parent_w, time_t the_time, void (*callback)( ), void *callback_data );
time_t gui_dateedit_get_time( GtkWidget *dateedit_w );
void gui_dateedit_set_time( GtkWidget *dateedit_w, time_t the_time );
GtkWidget *gui_entry_add( GtkWidget *parent_w, const char *init_text, void (*callback)( ), void *callback_data );
void gui_entry_set_text( GtkWidget *entry_w, const char *entry_text );
char *gui_entry_get_text( GtkWidget *entry_w );
void gui_entry_highlight( GtkWidget *entry_w );
GtkWidget *gui_frame_add( GtkWidget *parent_w, const char *title );
GtkWidget *gui_gl_area_add( GtkWidget *parent_w );
void gui_keybind( GtkWidget *widget, char *keystroke );
GtkWidget *gui_label_add( GtkWidget *parent_w, const char *label_text );
GtkWidget *gui_menu_add( GtkWidget *parent_menu_w, const char *label );
GtkWidget *gui_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data );
void gui_radio_menu_begin( int init_selected );
GtkWidget *gui_radio_menu_item_add( GtkWidget *menu_w, const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_option_menu_add( GtkWidget *parent_w, int init_selected );
GtkWidget *gui_option_menu_item( const char *label, void (*callback)( ), void *callback_data );
GtkWidget *gui_notebook_add( GtkWidget *parent_w );
void gui_notebook_page_add( GtkWidget *notebook_w, const char *tab_label, GtkWidget *content_w );
GtkWidget *gui_hpaned_add( GtkWidget *parent_w, int divider_x_pos );
GtkWidget *gui_vpaned_add( GtkWidget *parent_w, int divider_y_pos );
GtkWidget *gui_pixmap_xpm_add( GtkWidget *parent_w, char **xpm_data );
GtkWidget *gui_preview_add( GtkWidget *parent_w );
void gui_preview_spectrum( GtkWidget *preview_w, RGBcolor (*spectrum_func)( double x ) );
GtkWidget *gui_hscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment );
GtkWidget *gui_vscrollbar_add( GtkWidget *parent_w, GtkAdjustment *adjustment );
GtkWidget *gui_separator_add( GtkWidget *parent_w );
GtkWidget *gui_statusbar_add( GtkWidget *parent_w );
void gui_statusbar_message( GtkWidget *statusbar_w, const char *message );
GtkWidget *gui_table_add( GtkWidget *parent_w, int num_rows, int num_cols, boolean homog, int cell_padding );
void gui_table_attach( GtkWidget *table_w, GtkWidget *widget, int left, int right, int top, int bottom );
GtkWidget *gui_text_area_add( GtkWidget *parent_w, const char *init_text );
void gui_widget_packing( GtkWidget *widget, boolean expand, boolean fill, boolean start );
GtkWidget *gui_colorsel_window( const char *title, RGBcolor *init_color, void (*ok_callback)( ), void *ok_callback_data );
GtkWidget *gui_dialog_window( const char *title, void (*close_callback )( ) );
GtkWidget *gui_entry_window( const char *title, const char *init_text, void (*ok_callback)( ), void *ok_callback_data );
GtkWidget *gui_filesel_window( const char *title, const char *init_filename, void (*ok_callback)( ), void *ok_callback_data );
void gui_window_icon_xpm( GtkWidget *window_w, char **xpm_data );
void gui_window_modalize( GtkWidget *window_w, GtkWidget *parent_window_w );
#endif /* __GTK_H__ */


/* end gui.h */
