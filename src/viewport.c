/* viewport.c */

/* Viewport routines */

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
#include "viewport.h"

#include <gtk/gtk.h>
#include <GL/gl.h> /* GLuint */

#include "about.h"
#include "camera.h"
#include "dialog.h" /* context_menu( ) */
#include "filelist.h" /* filelist_show_entry( ) */
#include "geometry.h"
#include "gui.h"
#include "ogl.h"
#include "window.h"


/* Sensitivity factor used for manual camera control */
#define MOUSE_SENSITIVITY 0.5


/* The node table, used to find a node by its ID number */
static GNode **node_table = NULL;

/* The currently highlighted (indicated) node */
static GNode *indicated_node = NULL;

/* Previous mouse pointer coordinates */
static int prev_x = 0, prev_y = 0;


/* Receives a newly created node table from scanfs( ) */
void
viewport_pass_node_table( GNode **new_node_table )
{
	if (node_table != NULL)
		xfree( node_table );

	node_table = new_node_table;
}


/* This returns the node (if any) that is visible at viewport location
 * (x,y) (where (0,0) indicates the upper-left corner). The ID number of
 * the particular face being pointed at is stored in face_id */
static GNode *
node_at_location( int x, int y, unsigned int *face_id )
{
	const GLuint *hit_records;
	unsigned int name_count, z1, z2, name1, name2;
	unsigned int min_z1;
	unsigned int node_id = 0;
	int hit_count;
	int i = 0;

	*face_id = 0;

	hit_count = ogl_select( x, y, &hit_records );
	if (hit_count > 0) {
		/* Process selection hit records: find nearest hit
		 * (i.e. hit with smallest window-coordinate z value) */
		min_z1 = 4294967295U; /* 2^32 - 1 */
		while (hit_count > 0) {
			name_count = hit_records[i++];
			z1 = hit_records[i++];
			z2 = hit_records[i++];
			name1 = hit_records[i++];
			if (name_count == 2)
				name2 = hit_records[i++];
			else
				name2 = 0; /* default */
			if (z1 < min_z1) {
				node_id = name1;
				*face_id = name2;
				min_z1 = z1;
			}
			z2 = z2; /* z2 not used */
			--hit_count;
		}

		if (node_id != 0)
			return node_table[node_id];
	}

	return NULL;
}


/* This callback catches all events for the viewport */
int
viewport_cb( GtkWidget *gl_area_w, GdkEvent *event )
{
	GdkEventButton *ev_button;
	GdkEventMotion *ev_motion;
	GNode *node;
	double dx, dy;
	unsigned int face_id;
	int x, y;
	boolean btn1, btn2, btn3;
	boolean ctrl_key;

	/* Handle low-level GL area widget events */
	switch (event->type) {
		case GDK_EXPOSE:
		ogl_refresh( );
		return FALSE;

		case GDK_CONFIGURE:
		ogl_resize( );
		return FALSE;

		default:
		/* Event is probably coming from the mouse */
		break;
	}

	if (event->type == GDK_BUTTON_PRESS) {
		/* Exit the About presentation if it is up */
		if (about( ABOUT_END )) {
			indicated_node = NULL;
			return FALSE;
		}
	}

	/* If we're in splash screen mode, proceed no further */
	if (globals.fsv_mode == FSV_SPLASH)
		return FALSE;

	/* Mouse-related events */
	switch (event->type) {
		case GDK_BUTTON_PRESS:
		ev_button = (GdkEventButton *)event;
		btn1 = ev_button->button == 1;
		btn2 = ev_button->button == 2;
		btn3 = ev_button->button == 3;
		ctrl_key = ev_button->state & GDK_CONTROL_MASK;
		x = (int)ev_button->x;
		y = (int)ev_button->y;
		if (camera_moving( )) {
			/* Yipe! Impatient user */
			camera_pan_finish( );
			indicated_node = NULL;
		}
		else if (!ctrl_key) {
			if (btn2)
				indicated_node = NULL;
			else
				indicated_node = node_at_location( x, y, &face_id );
			if (indicated_node == NULL) {
				geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, "" );
			}
			else {
				if (geometry_should_highlight( indicated_node, face_id ) || btn1)
					geometry_highlight_node( indicated_node, btn1 );
				else
					geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
				if (btn3) {
					/* Bring up context-sensitive menu */
					context_menu( indicated_node, ev_button );
					filelist_show_entry( indicated_node );
				}
			}
		}
		prev_x = x;
		prev_y = y;
		break;

		case GDK_2BUTTON_PRESS:
		/* Ignore second click of a double-click */
		break;

		case GDK_BUTTON_RELEASE:
		ev_button = (GdkEventButton *)event;
		btn1 = ev_button->state & GDK_BUTTON1_MASK;
		ctrl_key = ev_button->state & GDK_CONTROL_MASK;
		if (btn1 && !ctrl_key && !camera_moving( ) && (indicated_node != NULL))
			camera_look_at( indicated_node );
		gui_cursor( gl_area_w, -1 );
		break;

		case GDK_MOTION_NOTIFY:
		ev_motion = (GdkEventMotion *)event;
		btn1 = ev_motion->state & GDK_BUTTON1_MASK;
		btn2 = ev_motion->state & GDK_BUTTON2_MASK;
		btn3 = ev_motion->state & GDK_BUTTON3_MASK;
		ctrl_key = ev_motion->state & GDK_CONTROL_MASK;
		x = (int)ev_motion->x;
		y = (int)ev_motion->y;
		if (!camera_moving( ) && !gtk_events_pending( )) {
			if (btn2) {
				/* Dolly the camera */
				gui_cursor( gl_area_w, GDK_DOUBLE_ARROW );
				dy = MOUSE_SENSITIVITY * (y - prev_y);
				camera_dolly( - dy );
				indicated_node = NULL;
			}
			else if (ctrl_key && btn1) {
				/* Revolve the camera */
				gui_cursor( gl_area_w, GDK_FLEUR );
				dx = MOUSE_SENSITIVITY * (x - prev_x);
				dy = MOUSE_SENSITIVITY * (y - prev_y);
				camera_revolve( dx, dy );
				indicated_node = NULL;
			}
			else if (!ctrl_key && (btn1 || btn3)) {
				/* Pointless dragging */
				if (indicated_node != NULL) {
					node = node_at_location( x, y, &face_id );
					if (node != indicated_node)
						indicated_node = NULL;
				}
			}
                        else
				indicated_node = node_at_location( x, y, &face_id );
			/* Update node highlighting */
			if (indicated_node == NULL) {
				geometry_highlight_node( NULL, FALSE );
				window_statusbar( SB_RIGHT, "" );
			}
			else {
				if (geometry_should_highlight( indicated_node, face_id ) || btn1)
					geometry_highlight_node( indicated_node, btn1 );
				else
					geometry_highlight_node( NULL, FALSE);
				window_statusbar( SB_RIGHT, node_absname( indicated_node ) );
			}
			prev_x = x;
			prev_y = y;
		}
		break;

		case GDK_LEAVE_NOTIFY:
		/* The mouse has left the viewport */
		geometry_highlight_node( NULL, FALSE );
		window_statusbar( SB_RIGHT, "" );
		gui_cursor( gl_area_w, -1 );
		indicated_node = NULL;
		break;

		default:
		/* Ignore event */
		break;
	}

	return FALSE;
}


/* end viewport.c */
