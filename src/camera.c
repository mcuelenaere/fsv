/* camera.c */

/* Camera control */

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
#include "camera.h"

#include <gtk/gtk.h>

#include "animation.h"
#include "dirtree.h" /* dirtree_entry_expanded( ) */
#include "filelist.h"
#include "geometry.h"
#include "gui.h"
#include "window.h"


/* Lower/upper bounds on pan times (in seconds) */
#define DISCV_CAMERA_MIN_PAN_TIME	0.5
#define DISCV_CAMERA_MAX_PAN_TIME	3.0
#define MAPV_CAMERA_MIN_PAN_TIME	0.5
#define MAPV_CAMERA_MAX_PAN_TIME	4.0
#define TREEV_CAMERA_MIN_PAN_TIME	1.0
#define TREEV_CAMERA_MAX_PAN_TIME	4.0

#define TREEV_CAMERA_AVG_VELOCITY	1024.0


/* Used in scrollbar routines */
enum {
	X_AXIS,
	Y_AXIS
};


/* The camera */
static union AnyCamera the_camera;

/* More convenient pointer to the camera */
Camera *camera = CAMERA(&the_camera);

/* Viewport scrollbar adjustments */
static GtkAdjustment *x_scrollbar_adj;
static GtkAdjustment *y_scrollbar_adj;

/* Scrollbar adjustments at outset of a camera pan */
static GtkAdjustment prev_x_scrollbar_adj;
static GtkAdjustment prev_y_scrollbar_adj;

/* Strings for message passing */
static char x_axis_mesg[] = "x_axis";
static char y_axis_mesg[] = "y_axis";

/* TRUE if the camera is currently moving */
static boolean camera_currently_moving = FALSE;

/* Camera state prior to entering bird's-eye-view mode */
static union AnyCamera pre_birdseye_view_camera;

/* TRUE if in bird's-eye-view mode */
static boolean birdseye_view_active = FALSE;


/* External interface to check if camera is in motion */
boolean
camera_moving( void )
{
	return camera_currently_moving;
}


/* Returns the diameter of a camera's visible range (centered at the
 * target) given the specified field of view and distance to target */
static double
field_diameter( double fov, double distance )
{
	return (2.0 * distance * tan( RAD(0.5 * fov) ));
}


/* Returns the distance that a camera with the given field of view must
 * have to a target object of the specified diameter, if the target object
 * is to fill the field of view (inverse of field_diameter( )) */
static double
field_distance( double fov, double diameter )
{
	return (diameter * (0.5 / tan( RAD(0.5 * fov) )));
}


/* Initializes camera state for the specified mode. initial_view flag
 * specifies whether camera is being initialized for the first time for
 * a particular filesystem (FALSE e.g. after switching the vis mode) */
void
camera_init( FsvMode mode, boolean initial_view )
{
	RTvec ext_c1;
	double d, d1, d2;

	camera->fov = 60.0;
	camera->pan_part = 1.0;
	switch (mode) {
		case FSV_DISCV:
		d = field_distance( camera->fov, 2.0 * DISCV_GEOM_PARAMS(root_dnode)->radius );
		if (initial_view) {
			camera->distance = 2.0 * d;
			DISCV_CAMERA(camera)->target.x = 0.0;
			DISCV_CAMERA(camera)->target.y = 0.0;
		}
		else {
			camera->distance = 3.0 * d;
			DISCV_CAMERA(camera)->target.x = 0.0;
			DISCV_CAMERA(camera)->target.y = 0.0;
		}
		camera->near_clip = 0.9375 * camera->distance;
		camera->far_clip = 1.0625 * camera->distance;
		break;

		case FSV_MAPV:
		d1 = field_distance( camera->fov, MAPV_NODE_WIDTH(root_dnode) );
		d2 = MAPV_GEOM_PARAMS(root_dnode)->height + geometry_mapv_max_expanded_height( root_dnode );
		d = MAX(d1, d2);
		if (initial_view) {
			camera->theta = 270.0;
			camera->phi = 0.0;
			camera->distance = 4.0 * d;
			MAPV_CAMERA(camera)->target.x = 0.0;
			MAPV_CAMERA(camera)->target.y = 0.0;
			MAPV_CAMERA(camera)->target.z = 0.0;
		}
		else {
			if (globals.current_node == root_dnode) {
				camera->theta = 270.0;
				camera->phi = 90.0;
				camera->distance = 1.05 * d2 / NEAR_TO_DISTANCE_RATIO;
				MAPV_CAMERA(camera)->target.x = 0.0;
				MAPV_CAMERA(camera)->target.y = MAPV_GEOM_PARAMS(root_dnode)->c1.y + camera->distance;
				MAPV_CAMERA(camera)->target.z = 0.0;
			}
			else {
				camera->theta = 270.0;
				camera->phi = 90.0;
				camera->distance = 1.5 * d;
				MAPV_CAMERA(camera)->target.x = 0.0;
				MAPV_CAMERA(camera)->target.y = 0.0;
				MAPV_CAMERA(camera)->target.z = 0.0;
			}
		}
		camera->near_clip = NEAR_TO_DISTANCE_RATIO * camera->distance;
		camera->far_clip = FAR_TO_NEAR_RATIO * camera->near_clip;
		break;

		case FSV_TREEV:
		geometry_treev_get_extents( root_dnode, NULL, &ext_c1 );
		d = field_distance( camera->fov, 2.0 * ext_c1.r );
		if (initial_view) {
			camera->theta = 0.0;
			camera->phi = 90.0;
			camera->distance = 2.0 * d;
			TREEV_CAMERA(camera)->target.r = 0.5 * TREEV_GEOM_PARAMS(root_dnode)->platform.depth + geometry_treev_platform_r0( root_dnode );
			TREEV_CAMERA(camera)->target.theta = 90.0;
			TREEV_CAMERA(camera)->target.z = 0.0;
		}
		else {
			camera->theta = 0.0;
			camera->phi = 90.0;
			camera->distance = d;
			TREEV_CAMERA(camera)->target.r = 0.0;
			TREEV_CAMERA(camera)->target.theta = 90.0;
			TREEV_CAMERA(camera)->target.z = 0.0;
		}
		camera->near_clip = NEAR_TO_DISTANCE_RATIO * camera->distance;
		camera->far_clip = FAR_TO_NEAR_RATIO * camera->near_clip;
		break;

                SWITCH_FAIL
	}
}


/* Formula for camera yaw in MapV mode */
static double
mapv_camera_theta( double target_x )
{
	return 270.0 + 45.0 * target_x / MAPV_NODE_WIDTH(root_dnode);
}


/* Formula for camera pitch in MapV mode */
static double
mapv_camera_phi( double target_y, GNode *target_node )
{
	if (target_node == root_dnode)
		return 52.5;

	return 45.0 + 15.0 * (target_y - MAPV_GEOM_PARAMS(target_node->parent)->c0.y) / MAPV_NODE_DEPTH(target_node->parent);
}


/* Formula for camera yaw in TreeV mode */
static double
treev_camera_theta( double target_theta, GNode *target_node )
{
	double rel_theta;

	if (geometry_treev_is_leaf( target_node )) {
                rel_theta = target_theta - geometry_treev_platform_theta( target_node->parent );
		return -15.0 * rel_theta / TREEV_GEOM_PARAMS(target_node->parent)->platform.arc_width;
	}
	else
		return -0.125 * (target_theta - 90.0);
}


/* Helper function for camera_scrollbar_move_cb( ) */
static void
discv_scrollbar_move( double value, int axis )
{
	switch (axis) {
		case X_AXIS:
		/* ????? */
		value = value;
		break;

		case Y_AXIS:
		/* ????? */
		value = value;
		break;

		SWITCH_FAIL
	}
}


/* Helper function for camera_scrollbar_move_cb( ) */
static void
mapv_scrollbar_move( double value, int axis )
{
	switch (axis) {
		case X_AXIS:
		MAPV_CAMERA(camera)->target.x = value;
		if (!birdseye_view_active) {
			/* Yaw appropriately */
			camera->theta = mapv_camera_theta( value );
		}
		break;

		case Y_AXIS:
		MAPV_CAMERA(camera)->target.y = - value;
		if (!birdseye_view_active && (globals.current_node != root_dnode)) {
			/* Pitch appropriately */
			camera->phi = mapv_camera_phi( - value, globals.current_node );
		}
		break;

		SWITCH_FAIL
	}
}


/* Helper function for camera_scrollbar_move_cb( ) */
static void
treev_scrollbar_move( double value, int axis )
{
	switch (axis) {
		case X_AXIS:
		TREEV_CAMERA(camera)->target.theta = - value;
		if (birdseye_view_active) {
			/* Keep root directory at the 12-o'clock position */
			camera->theta = 90.0 - TREEV_CAMERA(camera)->target.theta;
		}
		else {
			/* Yaw appropriately */
			camera->theta = treev_camera_theta( - value, globals.current_node );
		}
		break;

		case Y_AXIS:
		TREEV_CAMERA(camera)->target.r = - value;
		break;

		SWITCH_FAIL
	}
}


/* This callback services the viewport scrollbars (or, more precisely,
 * their adjustments) whenever either is moved by the user */
static void
camera_scrollbar_move_cb( GtkAdjustment *adj, const char *mesg )
{
	double value;
	int axis;

	/* Get value at center of scrollbar slider */
	value = adj->value + 0.5 * adj->page_size;

	if (!strcmp( mesg, x_axis_mesg ))
                axis = X_AXIS;
	else if (!strcmp( mesg, y_axis_mesg ))
		axis = Y_AXIS;
	else {
		g_assert_not_reached( );
		return;
	}

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		discv_scrollbar_move( value, axis );
		break;

		case FSV_MAPV:
		mapv_scrollbar_move( value, axis );
		break;

		case FSV_TREEV:
		treev_scrollbar_move( value, axis );
		break;

		SWITCH_FAIL
	}

	/* Camera is under user control */
	camera->manual_control = TRUE;

	redraw( );
}


/* Correspondence from window_init( ) */
void
camera_pass_scrollbar_widgets( GtkWidget *x_scrollbar_w, GtkWidget *y_scrollbar_w )
{
	/* Get the adjustments */
	x_scrollbar_adj = GTK_RANGE(x_scrollbar_w)->adjustment;
	y_scrollbar_adj = GTK_RANGE(y_scrollbar_w)->adjustment;

	/* Connect signal handlers */
	gtk_signal_connect( GTK_OBJECT(x_scrollbar_adj), "value_changed", GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), x_axis_mesg );
	gtk_signal_connect( GTK_OBJECT(y_scrollbar_adj), "value_changed", GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), y_axis_mesg );
}


/* Default scrollbar states */
static void
null_get_scrollbar_states( GtkAdjustment *x_adj, GtkAdjustment *y_adj )
{
	x_adj->lower = 0.0;
	x_adj->upper = 100.0;
	x_adj->value = 0.0;
	x_adj->step_increment = 0.0;
	x_adj->page_increment = 0.0;
	x_adj->page_size = 100.0;
        *y_adj = *x_adj; /* struct assign */
}


/* This produces the exact state the viewport scrollbars should have in
 * DiscV mode, given the current camera state and current node */
static void
discv_get_scrollbar_states( GtkAdjustment *x_adj, GtkAdjustment *y_adj )
{

	/* TODO: To be implemented... */

	*x_adj = *x_scrollbar_adj; /* struct assign */
	*y_adj = *y_scrollbar_adj; /* struct assign */
}


/* Same as above, but for MapV mode */
static void
mapv_get_scrollbar_states( GtkAdjustment *x_adj, GtkAdjustment *y_adj )
{
	GNode *dnode;
	XYvec dims, margin;
	XYvec c0, c1;
	double diameter;
	double cofs;

	if (birdseye_view_active) {
		/* The bird sees everything */
		dnode = root_dnode;
	}
	else {
		/* Scrollable area is that of top face of parent
		 * directory of the current node */
		if (NODE_IS_DIR(globals.current_node->parent))
			dnode = globals.current_node->parent;
		else
			dnode = globals.current_node;
	}

	/* Dimensions of scrollable area */
	dims.x = MAPV_NODE_WIDTH(dnode);
	dims.y = MAPV_NODE_DEPTH(dnode);

	/* Diameter of camera's field of view (centered at target) */
	diameter = field_diameter( camera->fov, camera->distance );

	/* Margin widths */
	margin.x = 0.5 * MIN(diameter, dims.x);
	margin.y = 0.5 * MIN(diameter, dims.y);

	/* Corners of scrollable area */
	c0.x = MIN(MAPV_GEOM_PARAMS(dnode)->c0.x + margin.x, MAPV_CAMERA(camera)->target.x);
	c0.y = MIN(MAPV_GEOM_PARAMS(dnode)->c0.y + margin.y, MAPV_CAMERA(camera)->target.y);
	c1.x = MAX(MAPV_GEOM_PARAMS(dnode)->c1.x - margin.x, MAPV_CAMERA(camera)->target.x);
	c1.y = MAX(MAPV_GEOM_PARAMS(dnode)->c1.y - margin.y, MAPV_CAMERA(camera)->target.y);

	/* Corrective offset (since adj->value actually indicates position
	 * at top of scrollbar slider, not center) */
	cofs = 0.5 * diameter;

	/* x-scrollbar state */
	x_adj->lower = c0.x - cofs;
	x_adj->upper = c1.x + cofs;
	x_adj->value = MAPV_CAMERA(camera)->target.x - cofs;
	x_adj->step_increment = dims.x / 256.0;
	x_adj->page_increment = dims.x / 16.0;
	x_adj->page_size = diameter;

	/* y-scrollbar state
	 * Note: lower, upper, and value have signs reversed to correct for
	 * canonical scrollbar increment direction (wrong for our needs) */
	y_adj->lower = - c1.y - cofs;
	y_adj->upper = - c0.y + cofs;
	y_adj->value = - MAPV_CAMERA(camera)->target.y - cofs;
	y_adj->step_increment = dims.y / 256.0;
	y_adj->page_increment = dims.y / 16.0;
	y_adj->page_size = diameter;
}


/* Same as above, but for TreeV mode */
static void
treev_get_scrollbar_states( GtkAdjustment *x_adj, GtkAdjustment *y_adj )
{
	GNode *dnode;
	RTvec area_dims, dir_pos;
	RTvec c0, c1;
	RTvec vis_range;
	RTvec margin;
	double diameter;
	double cofs;

	if (!dirtree_entry_expanded( root_dnode )) {
		/* Disable scrolling in this circumstance */
		null_get_scrollbar_states( x_adj, y_adj );
                return;
	}

	/* Get dimensions of scrollable area */
	if (birdseye_view_active) {
		/* Birdie can fly around the entire tree */
		if (geometry_treev_is_leaf( globals.current_node ))
			area_dims.r = geometry_treev_platform_r0( globals.current_node->parent );
		else
			area_dims.r = geometry_treev_platform_r0( globals.current_node );
		dnode = root_dnode;
		area_dims.theta = MAX(TREEV_GEOM_PARAMS(dnode)->platform.arc_width, TREEV_GEOM_PARAMS(dnode)->platform.subtree_arc_width);
	}
	else {
		if (geometry_treev_is_leaf( globals.current_node )) {
			dnode = globals.current_node->parent;
			area_dims.theta = TREEV_GEOM_PARAMS(dnode)->platform.arc_width;
		}
		else {
			dnode = globals.current_node;
			area_dims.theta = MAX(TREEV_GEOM_PARAMS(dnode)->platform.arc_width, TREEV_GEOM_PARAMS(dnode)->platform.subtree_arc_width);
		}
		area_dims.r = TREEV_GEOM_PARAMS(dnode)->platform.depth;
	}

	/* Visible range of camera's field of view */
	diameter = field_diameter( camera->fov, camera->distance );
	vis_range.r = diameter;
	vis_range.theta = (180.0 / PI) * diameter / TREEV_CAMERA(camera)->target.r;

	/* Margin width (r-axis only; angle margin doesn't work well
	 * with camera yaw) */
	margin.r = 0.5 * MIN(vis_range.r, area_dims.r);

	/* Base directory position */
	dir_pos.r = geometry_treev_platform_r0( dnode );
	dir_pos.theta = geometry_treev_platform_theta( dnode );

	/* Corners of scrollable area */
	c0.r = MIN(dir_pos.r + margin.r, TREEV_CAMERA(camera)->target.r);
	c0.theta = MIN(dir_pos.theta - 0.5 * area_dims.theta, TREEV_CAMERA(camera)->target.theta);
	c1.r = MAX(dir_pos.r + area_dims.r - margin.r, TREEV_CAMERA(camera)->target.r);
	c1.theta = MAX(dir_pos.theta + 0.5 * area_dims.theta, TREEV_CAMERA(camera)->target.theta);

	/* x-scrollbar state (signs reversed) */
	cofs = 0.5 * vis_range.theta;
	x_adj->lower = - c1.theta - cofs;
	x_adj->upper = - c0.theta + cofs;
	x_adj->value = - TREEV_CAMERA(camera)->target.theta - cofs;
	x_adj->step_increment = area_dims.theta / 256.0;
	x_adj->page_increment = area_dims.theta / 16.0;
	x_adj->page_size = vis_range.theta;

	/* y-scrollbar state (signs reversed) */
	cofs = 0.5 * vis_range.r;
	y_adj->lower = - c1.r - cofs;
	y_adj->upper = - c0.r + cofs;
	y_adj->value = - TREEV_CAMERA(camera)->target.r - cofs;
	y_adj->step_increment = area_dims.r / 256.0;
	y_adj->page_increment = area_dims.r / 16.0;
	y_adj->page_size = vis_range.r;
}


/* Copies the values of from_adj into to_adj
 * (cannot do a struct assign, otherwise ID info is copied as well) */
static void
adj_copy( GtkAdjustment *to_adj, GtkAdjustment *from_adj )
{
	to_adj->lower = from_adj->lower;
	to_adj->upper = from_adj->upper;
	to_adj->value = from_adj->value;
	to_adj->step_increment = from_adj->step_increment;
	to_adj->page_increment = from_adj->page_increment;
	to_adj->page_size = from_adj->page_size;
}


/* This sets the values of adj to be somewhere between the corresponding
 * values of a_adj and b_adj, specified by the interpolation factor k
 * (i.e. if k == 0, then adj = a_adj; if k == 1, then adj = b_adj, etc.
 * k should be between 0 and 1 inclusive, of course) */
static void
adj_interpolate( GtkAdjustment *adj, double k, GtkAdjustment *a_adj, GtkAdjustment *b_adj )
{
	adj->lower = a_adj->lower + k * (b_adj->lower - a_adj->lower);
	adj->upper = a_adj->upper + k * (b_adj->upper - a_adj->upper);
	adj->value = a_adj->value + k * (b_adj->value - a_adj->value);
	adj->step_increment = a_adj->step_increment + k * (b_adj->step_increment - a_adj->step_increment);
	adj->page_increment = a_adj->page_increment + k * (b_adj->page_increment - a_adj->page_increment);
	adj->page_size = a_adj->page_size + k * (b_adj->page_size - a_adj->page_size);
}


/* Updates the scrollbars to reflect current camera state and current node
 * status (interpolating smoothly if the camera is panning).
 * hard_update indicates if scrollbars must be updated now (TRUE) or may be
 * updated later (FALSE) */
void
camera_update_scrollbars( boolean hard_update )
{
	GtkAdjustment x_adj, y_adj;

	/* Get current scrollbar states */
	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		null_get_scrollbar_states( &x_adj, &y_adj );
		break;

		case FSV_DISCV:
		discv_get_scrollbar_states( &x_adj, &y_adj );
		break;

		case FSV_MAPV:
		mapv_get_scrollbar_states( &x_adj, &y_adj );
		break;

		case FSV_TREEV:
		treev_get_scrollbar_states( &x_adj, &y_adj );
		break;

		SWITCH_FAIL
	}

	if (camera_moving( )) {
		/* Interpolate between current and previous scrollbar
		 * states according to position in camera pan */
		adj_interpolate( x_scrollbar_adj, camera->pan_part, &prev_x_scrollbar_adj, &x_adj );
		adj_interpolate( y_scrollbar_adj, camera->pan_part, &prev_y_scrollbar_adj, &y_adj );
	}
	else {
		/* Use scrollbar states as-is */
		adj_copy( x_scrollbar_adj, &x_adj );
		adj_copy( y_scrollbar_adj, &y_adj );
	}

	/* Update the scrollbar widgets */
	if (hard_update || !gui_adjustment_widget_busy( x_scrollbar_adj )) {
		gtk_signal_handler_block_by_func( GTK_OBJECT(x_scrollbar_adj), GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), x_axis_mesg );
		gtk_signal_emit_by_name( GTK_OBJECT(x_scrollbar_adj), "changed" );
		gtk_signal_handler_unblock_by_func( GTK_OBJECT(x_scrollbar_adj), GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), x_axis_mesg );
	}
	if (hard_update || !gui_adjustment_widget_busy( y_scrollbar_adj )) {
		gtk_signal_handler_block_by_func( GTK_OBJECT(y_scrollbar_adj), GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), y_axis_mesg );
		gtk_signal_emit_by_name( GTK_OBJECT(y_scrollbar_adj), "changed" );
		gtk_signal_handler_unblock_by_func( GTK_OBJECT(y_scrollbar_adj), GTK_SIGNAL_FUNC(camera_scrollbar_move_cb), y_axis_mesg );
	}
}


/* This causes an ongoing camera pan to finish immediately
 * (i.e. camera jumps instantly to its destination) */
void
camera_pan_finish( void )
{
	morph_finish( &camera->theta );
	morph_finish( &camera->phi );
	morph_finish( &camera->distance );
	morph_finish( &camera->fov );
	morph_finish( &camera->near_clip );
	morph_finish( &camera->far_clip );
	morph_finish( &camera->pan_part );

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		morph_finish( &DISCV_CAMERA(camera)->target.x );
		morph_finish( &DISCV_CAMERA(camera)->target.y );
		break;

		case FSV_MAPV:
		morph_finish( &MAPV_CAMERA(camera)->target.x );
		morph_finish( &MAPV_CAMERA(camera)->target.y );
		morph_finish( &MAPV_CAMERA(camera)->target.z );
		break;

		case FSV_TREEV:
		morph_finish( &TREEV_CAMERA(camera)->target.r );
		morph_finish( &TREEV_CAMERA(camera)->target.theta );
		morph_finish( &TREEV_CAMERA(camera)->target.z );
		break;

		SWITCH_FAIL
	}
}


/* This stops an ongoing camera pan immediately
 * (camera does not reach its destination) */
void
camera_pan_break( void )
{
	morph_break( &camera->theta );
	morph_break( &camera->phi );
	morph_break( &camera->distance );
	morph_break( &camera->fov );
	morph_break( &camera->near_clip );
	morph_break( &camera->far_clip );
	morph_break( &camera->pan_part );

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		morph_break( &DISCV_CAMERA(camera)->target.x );
		morph_break( &DISCV_CAMERA(camera)->target.y );
		break;

		case FSV_MAPV:
		morph_break( &MAPV_CAMERA(camera)->target.x );
		morph_break( &MAPV_CAMERA(camera)->target.y );
		morph_break( &MAPV_CAMERA(camera)->target.z );
		break;

		case FSV_TREEV:
		morph_break( &TREEV_CAMERA(camera)->target.r );
		morph_break( &TREEV_CAMERA(camera)->target.theta );
		morph_break( &TREEV_CAMERA(camera)->target.z );
		break;

		SWITCH_FAIL
	}
}


/* Helper function for camera_look_at_full( ) */
static double
discv_look_at( GNode *node, MorphType mtype, double pan_time_override )
{
	DiscVCamera new_dcam;
	Camera *new_cam;
	XYvec *node_pos;
	double pan_time;

	new_cam = CAMERA(&new_dcam);

	/* Construct desired camera state */

	/* Distance from target point */
	new_cam->distance = 2.0 * field_distance( camera->fov, 2.0 * DISCV_GEOM_PARAMS(node)->radius );

	/* Clipping plane distances */
	new_cam->near_clip = 0.9375 * new_cam->distance;
	new_cam->far_clip = 1.0625 * new_cam->distance;

	/* Target point */
	node_pos = geometry_discv_node_pos( node );
	DISCV_CAMERA(new_cam)->target.x = node_pos->x;
	DISCV_CAMERA(new_cam)->target.y = node_pos->y;

	/* Duration of pan */
	if (pan_time_override > 0.0)
		pan_time = pan_time_override;
	else {
/* TODO: write a *real* pan_time function here */
		pan_time = 2.0;
		/*pan_time = CLAMP(k, DISCV_CAMERA_MIN_PAN_TIME, DISCV_CAMERA_MAX_PAN_TIME);*/
	}

	/* Get the camera moving */
	morph( &camera->distance, mtype, new_cam->distance, pan_time );
	morph( &camera->near_clip, mtype, new_cam->near_clip, pan_time );
	morph( &camera->far_clip, mtype, new_cam->far_clip, pan_time );
	morph( &DISCV_CAMERA(camera)->target.x, mtype, DISCV_CAMERA(new_cam)->target.x, pan_time );
	morph( &DISCV_CAMERA(camera)->target.y, mtype, DISCV_CAMERA(new_cam)->target.y, pan_time );

	return pan_time;
}


/* Helper function for mapv_look_at( ). Calculates the position of a camera
 * (i.e. viewer location) */
static void
mapv_get_camera_position( const Camera *cam, XYZvec *pos )
{
	double sin_theta, cos_theta, sin_phi, cos_phi;

	sin_theta = sin( RAD(cam->theta) );
	cos_theta = cos( RAD(cam->theta) );
	sin_phi = sin( RAD(cam->phi) );
	cos_phi = cos( RAD(cam->phi) );

	pos->x = MAPV_CAMERA(cam)->target.x + cam->distance * cos_theta * cos_phi;
	pos->y = MAPV_CAMERA(cam)->target.y + cam->distance * sin_theta * cos_phi;
	pos->z = MAPV_CAMERA(cam)->target.z + cam->distance * sin_phi;
}


/* Helper function for camera_look_at_full( ) */
static double
mapv_look_at( GNode *node, MorphType mtype, double pan_time_override )
{
	MapVCamera new_mcam;
	Camera *new_cam;
	Camera apg_cam;
	XYZvec node_pos;
	XYZvec camera_pos, new_cam_pos, delta;
	XYvec node_dims;
	double diameter, height;
	double pan_time;
	double xy_travel;
	double k;
	boolean swing_back = FALSE;

	new_cam = CAMERA(&new_mcam);

	/* Get target node geometry */
	node_pos.x = 0.5 * (MAPV_GEOM_PARAMS(node)->c0.x + MAPV_GEOM_PARAMS(node)->c1.x);
	node_pos.y = 0.5 * (MAPV_GEOM_PARAMS(node)->c0.y + MAPV_GEOM_PARAMS(node)->c1.y);
	node_pos.z = geometry_mapv_node_z0( node ) + MAPV_GEOM_PARAMS(node)->height;
	node_dims.x = MAPV_NODE_WIDTH(node);
	node_dims.y = MAPV_NODE_DEPTH(node);

	/* Construct desired camera state */

	/* Target point (may get bumped upward; see further down) */
	MAPV_CAMERA(new_cam)->target.x = node_pos.x;
	MAPV_CAMERA(new_cam)->target.y = node_pos.y;
	MAPV_CAMERA(new_cam)->target.z = node_pos.z;

	/* Viewing angles */
	new_cam->theta = mapv_camera_theta( node_pos.x );
        new_cam->phi = mapv_camera_phi( node_pos.y, node );

	/* Distance from target point */
	k = sqrt( node_dims.x * node_dims.y );
	diameter = SQRT_2 * MAX(k, 0.5 * MAX(node_dims.x, node_dims.y));
	if (NODE_IS_DIR(node)) {
		height = geometry_mapv_max_expanded_height( node );
		diameter = MAX(diameter, height);
		if (dirtree_entry_expanded( node ))
			diameter = MAX(diameter, MAX(node_dims.x, 1.5 * node_dims.y));
		MAPV_CAMERA(new_cam)->target.z += 0.5 * height;
		k = 1.25;
	}
	else
		k = 2.0;
	new_cam->distance = k * field_distance( camera->fov, diameter );

	/* Clipping plane distances */
	new_cam->near_clip = NEAR_TO_DISTANCE_RATIO * new_cam->distance;
	new_cam->far_clip = FAR_TO_NEAR_RATIO * new_cam->near_clip;

	/* Overall travel vector */
	mapv_get_camera_position( camera, &camera_pos );
	mapv_get_camera_position( new_cam, &new_cam_pos );
	delta.x = new_cam_pos.x - camera_pos.x;
	delta.y = new_cam_pos.y - camera_pos.y;
	delta.z = new_cam_pos.z - camera_pos.z;

	/* Determine how long the camera should take to perform the pan,
	 * if no overriding value was given */
	if (pan_time_override > 0.0)
		pan_time = pan_time_override;
	else {
		k = sqrt( XYZ_LEN(delta) / hypot( MAPV_NODE_WIDTH(root_dnode), MAPV_NODE_DEPTH(root_dnode) ) );
		pan_time = MAX(MAPV_CAMERA_MIN_PAN_TIME, MIN(1.0, k) * MAPV_CAMERA_MAX_PAN_TIME);
	}

	/* Judge if camera should swing back during the pan, and if so,
	 * determine apogee parameters */
	xy_travel = XY_LEN(delta);
	if (xy_travel > (3.0 * MAX(camera->distance, new_cam->distance))) {
		swing_back = TRUE;
		apg_cam.distance = 1.2 * MAX(new_cam->distance, xy_travel);
		apg_cam.near_clip = NEAR_TO_DISTANCE_RATIO * apg_cam.distance;
		apg_cam.far_clip = FAR_TO_NEAR_RATIO * apg_cam.near_clip;
	}

	/* Get the camera moving */
	morph( &camera->theta, mtype, new_cam->theta, pan_time );
	morph( &camera->phi, mtype, new_cam->phi, pan_time );
	if (swing_back) {
		morph( &camera->distance, mtype, apg_cam.distance, 0.5 * pan_time );
		morph( &camera->distance, mtype, new_cam->distance, 0.5 * pan_time );
		morph( &camera->near_clip, mtype, apg_cam.near_clip, 0.5 * pan_time );
		morph( &camera->near_clip, mtype, new_cam->near_clip, 0.5 * pan_time );
		morph( &camera->far_clip, mtype, apg_cam.far_clip, 0.5 * pan_time );
		morph( &camera->far_clip, mtype, new_cam->far_clip, 0.5 * pan_time );
	}
	else {
		morph( &camera->distance, mtype, new_cam->distance, pan_time );
		morph( &camera->near_clip, mtype, new_cam->near_clip, pan_time );
		morph( &camera->far_clip, mtype, new_cam->far_clip, pan_time );
	}
	morph( &MAPV_CAMERA(camera)->target.x, mtype, MAPV_CAMERA(new_cam)->target.x, pan_time );
	morph( &MAPV_CAMERA(camera)->target.y, mtype, MAPV_CAMERA(new_cam)->target.y, pan_time );
	morph( &MAPV_CAMERA(camera)->target.z, mtype, MAPV_CAMERA(new_cam)->target.z, pan_time );

	return pan_time;
}


/* Helper function for treev_look_at( ). Calculates position of a camera. */
static void
treev_get_camera_position( const Camera *cam, RTZvec *pos )
{
	XYZvec target, xyz_pos;
	double theta;
	double sin_theta, cos_theta, sin_phi, cos_phi;

	/* Convert target from RTZ to XYZ */
	theta = TREEV_CAMERA(cam)->target.theta;
	target.x = TREEV_CAMERA(cam)->target.r * cos( RAD(theta) );
	target.y = TREEV_CAMERA(cam)->target.r * sin( RAD(theta) );
	target.z = TREEV_CAMERA(cam)->target.z;

	/* Absolute camera heading */
	theta = TREEV_CAMERA(cam)->target.theta + cam->theta - 180.0;
	sin_theta = sin( RAD(theta) );
	cos_theta = cos( RAD(theta) );
	sin_phi = sin( RAD(cam->phi) );
	cos_phi = cos( RAD(cam->phi) );

	/* XYZ position */
	xyz_pos.x = target.x + cam->distance * cos_theta * cos_phi;
	xyz_pos.y = target.y + cam->distance * sin_theta * cos_phi;
	xyz_pos.z = target.z + cam->distance * sin_phi;

	/* Convert position from XYZ to RTZ */
	pos->r = XY_LEN(xyz_pos);
	pos->theta = DEG(atan2( xyz_pos.y, xyz_pos.x ));
	pos->z = xyz_pos.z;
}


/* Helper function for camera_look_at_full( ) */
static double
treev_look_at( GNode *node, MorphType mtype, double pan_time_override )
{
	TreeVCamera new_tcam;
	Camera *new_cam;
	RTZvec camera_pos, new_cam_pos;
	double top_dist, height, diameter;
	double alpha;
	double pan_time;
	double k;

	new_cam = CAMERA(&new_tcam);

	/* Construct desired camera state */

	if (geometry_treev_is_leaf( node )) {
		/* Target point */
		TREEV_CAMERA(new_cam)->target.r = geometry_treev_platform_r0( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.distance;
		TREEV_CAMERA(new_cam)->target.theta = geometry_treev_platform_theta( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.theta;
		TREEV_CAMERA(new_cam)->target.z = TREEV_GEOM_PARAMS(node->parent)->platform.height + (MAGIC_NUMBER - 1.0) * TREEV_GEOM_PARAMS(node)->leaf.height;

		/* Distance from target point */
		top_dist = 2.5 * field_distance( camera->fov, (SQRT_2 * TREEV_LEAF_NODE_EDGE) );
		new_cam->distance = top_dist + (2.0 - MAGIC_NUMBER) * TREEV_GEOM_PARAMS(node)->leaf.height;

		/* Clipping plane distances */
		new_cam->near_clip = NEAR_TO_DISTANCE_RATIO * top_dist;
		new_cam->far_clip = FAR_TO_NEAR_RATIO * new_cam->near_clip;

		/* Viewing angles */
                new_cam->theta = treev_camera_theta( TREEV_CAMERA(new_cam)->target.theta, node );
		new_cam->phi = 45.0;
		/* Ensure that camera is pitched high enough to see top
		 * and bottom ends of leaf node */
		k = new_cam->distance * sin( RAD(0.25 * camera->fov) ) / ((2.0 - MAGIC_NUMBER) * TREEV_GEOM_PARAMS(node)->leaf.height);
		if ((k >= -1.0) && (k <= 1.0)) {
			alpha = DEG(asin( k )) - 0.25 * camera->fov;
			new_cam->phi = MAX(new_cam->phi, 90.0 - alpha);
		}
	}
	else {
		/* Target point */
		TREEV_CAMERA(new_cam)->target.r = geometry_treev_platform_r0( node ) + 0.3 * TREEV_GEOM_PARAMS(node)->platform.depth - (0.2 * TREEV_PLATFORM_SPACING_DEPTH);
		TREEV_CAMERA(new_cam)->target.theta = geometry_treev_platform_theta( node );
		TREEV_CAMERA(new_cam)->target.z = TREEV_GEOM_PARAMS(node)->platform.height;

		/* Distance from target point */
		height = geometry_treev_max_leaf_height( node );
		diameter = MAX(TREEV_GEOM_PARAMS(node)->platform.depth + (0.5 * TREEV_PLATFORM_SPACING_DEPTH), 0.25 * height);
		new_cam->distance = field_distance( camera->fov, diameter );

		/* Clipping plane distances */
		new_cam->near_clip = NEAR_TO_DISTANCE_RATIO * new_cam->distance;
		new_cam->far_clip = FAR_TO_NEAR_RATIO * new_cam->near_clip;

		/* Viewing angles */
		new_cam->theta = treev_camera_theta( TREEV_CAMERA(new_cam)->target.theta, node );
		new_cam->phi = 30.0;
	}

/* TODO: Implement swing_back for TreeV mode */

	/* Determine how long the camera should take to perform the pan,
	 * if no overriding value was given */
	if (pan_time_override > 0.0)
		pan_time = pan_time_override;
	else {
		treev_get_camera_position( camera, &camera_pos );
		treev_get_camera_position( new_cam, &new_cam_pos );
		k = RTZ_DIST(camera_pos, new_cam_pos) / TREEV_CAMERA_AVG_VELOCITY;
		pan_time = CLAMP(k, TREEV_CAMERA_MIN_PAN_TIME, TREEV_CAMERA_MAX_PAN_TIME);
	}

	/* Get the camera moving */
	morph( &camera->theta, mtype, new_cam->theta, pan_time );
	morph( &camera->phi, mtype, new_cam->phi, pan_time );
	morph( &camera->distance, mtype, new_cam->distance, pan_time );
	morph( &camera->near_clip, mtype, new_cam->near_clip, pan_time );
	morph( &camera->far_clip, mtype, new_cam->far_clip, pan_time );
	morph( &TREEV_CAMERA(camera)->target.r, mtype, TREEV_CAMERA(new_cam)->target.r, pan_time );
	morph( &TREEV_CAMERA(camera)->target.theta, mtype, TREEV_CAMERA(new_cam)->target.theta, pan_time );
	morph( &TREEV_CAMERA(camera)->target.z, mtype, TREEV_CAMERA(new_cam)->target.z, pan_time );

	return pan_time;
}


/* Step callback for camera panning */
static void
pan_step_cb( Morph *unused )
{
	globals.need_redraw = TRUE;
	camera_update_scrollbars( FALSE );
}


/* "Post-callback" for pan_end_cb( ), called exactly one frame later */
static void
post_pan_end( GNode *node )
{
	/* Inform geometry module of camera pan completion */
	geometry_camera_pan_finished( );

	/* Re-enable full user interface */
	window_set_access( TRUE );

	camera_update_scrollbars( TRUE );

	if (node != NULL) {
		/* Show entry for new current node */
		filelist_show_entry( node );
	}
}


/* End callback for camera panning */
static void
pan_end_cb( Morph *morph )
{
	GNode *node;

	globals.need_redraw = TRUE;

	node = (GNode *)morph->data;
	schedule_event( post_pan_end, node, 1 );

	camera_currently_moving = FALSE;
}


/* Points the camera at the given node, using the specified motion
 * morph type and (optionally, if value is nonnegative) the specified
 * pan duration */
void
camera_look_at_full( GNode *node, MorphType mtype, double pan_time_override )
{
	double pan_time = 0.0;
	GNode *prev_node = NULL;
	boolean backtracking = FALSE;

#ifdef DEBUG
	/* Parent directory of target node must be expanded
	 * (or at least be expanding) */
	if (NODE_IS_DIR(node->parent))
		g_assert( dirtree_entry_expanded( node->parent ) );
#endif

	/* Temporarily disable part of the user interface */
	window_set_access( FALSE );

	if (birdseye_view_active) {
		/* Leave bird's-eye view mode */
		window_birdseye_view_off( );
		birdseye_view_active = FALSE;
	}

	/* Save current scrollbar states */
	adj_copy( &prev_x_scrollbar_adj, x_scrollbar_adj );
	adj_copy( &prev_y_scrollbar_adj, y_scrollbar_adj );

	/* Halt any ongoing camera pan */
	camera_pan_break( );

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		pan_time = discv_look_at( node, mtype, pan_time_override );
		break;

		case FSV_MAPV:
		pan_time = mapv_look_at( node, mtype, pan_time_override );
		break;

		case FSV_TREEV:
		pan_time = treev_look_at( node, mtype, pan_time_override );
		break;

		SWITCH_FAIL
	}

	/* Master morph */
	camera->pan_part = 0.0;
	morph_full( &camera->pan_part, MORPH_LINEAR, 1.0, pan_time, pan_step_cb, pan_end_cb, node );

	/* Update visited node history */
	if (globals.history != NULL) {
		prev_node = (GNode *)globals.history->data;
		if (prev_node == NULL) {
			/* Camera is backtracking */
			G_LIST_REMOVE(globals.history, NULL);
			backtracking = TRUE;
		}
	}
	if (!backtracking && (node != globals.current_node) && (globals.current_node != prev_node))
		G_LIST_PREPEND(globals.history, globals.current_node);

	/* New current node of interest */
	globals.current_node = node;

	/* Camera is under our control now */
	camera->manual_control = FALSE;

	camera_currently_moving = TRUE;
}


/* This calls camera_look_at_full( ) with default arguments */
void
camera_look_at( GNode *node )
{
	camera_look_at_full( node, MORPH_SIGMOID, -1.0 );
}


/* Helper function for treev_look_at_lpan( ) */
static void
lpan_stage2( void **data )
{
	GNode *node;
	double pan_time;

	geometry_camera_pan_finished( );

	node = (GNode *)data[0];
	pan_time = *((double *)data[1]);
	xfree( data );

	camera_look_at_full( node, MORPH_SIGMOID, pan_time );
}


/* End callback used by camera_treev_lpan_look_at( ) */
static void
lpan_stage1_end_cb( Morph *morph )
{
	globals.need_redraw = TRUE;
	camera_update_scrollbars( FALSE );

	schedule_event( lpan_stage2, morph->data, 1 );
}


/* This points the camera at the given node, using a two-stage pan in which
 * the camera follows an L-shaped path ("lpan").
 * Note: currently implemented only for TreeV mode, 'cause that's the only
 * mode that uses it */
void
camera_treev_lpan_look_at( GNode *node, double pan_time_override )
{
	static double pan_time;
	TreeVCamera new_tcam;
	Camera *new_cam;
	RTZvec camera_pos, new_cam_pos;
	void **data;

	new_cam = CAMERA(&new_tcam);

	/* Disable part of user interface */
	window_set_access( FALSE );

	if (birdseye_view_active) {
		/* Leave bird's-eye view mode */
		window_birdseye_view_off( );
		birdseye_view_active = FALSE;
	}

	/* Construct desired camera state (stage 1) */

	if (geometry_treev_is_leaf( node )) {
		new_cam->theta = -15.0 * TREEV_GEOM_PARAMS(node)->leaf.theta / TREEV_GEOM_PARAMS(node->parent)->platform.arc_width;
		TREEV_CAMERA(new_cam)->target.r = geometry_treev_platform_r0( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.distance;
		TREEV_CAMERA(new_cam)->target.theta = geometry_treev_platform_theta( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.theta;
	}
	else {
		TREEV_CAMERA(new_cam)->target.r = geometry_treev_platform_r0( node ) + (2.0 - MAGIC_NUMBER) * TREEV_GEOM_PARAMS(node)->platform.depth;
		TREEV_CAMERA(new_cam)->target.theta = geometry_treev_platform_theta( node );
		new_cam->theta = -0.125 * (TREEV_CAMERA(new_cam)->target.theta - 90.0);
	}

	/* Duration of pan */
	if (pan_time_override > 0.0)
		pan_time = pan_time_override;
	else {
		treev_get_camera_position( camera, &camera_pos );
		treev_get_camera_position( new_cam, &new_cam_pos );
		pan_time = RTZ_DIST(camera_pos, new_cam_pos) / TREEV_CAMERA_AVG_VELOCITY;
		pan_time = CLAMP(pan_time, TREEV_CAMERA_MIN_PAN_TIME, TREEV_CAMERA_MAX_PAN_TIME);
	}

	camera_pan_break( );

	/* Get the camera moving */
	morph( &camera->theta, MORPH_INV_QUADRATIC, new_cam->theta, pan_time );
	morph( &TREEV_CAMERA(camera)->target.r, MORPH_INV_QUADRATIC, TREEV_CAMERA(new_cam)->target.r, pan_time );
	morph( &TREEV_CAMERA(camera)->target.theta, MORPH_INV_QUADRATIC, TREEV_CAMERA(new_cam)->target.theta, pan_time );

	/* Need to pass along both node and pan_time */
	data = NEW_ARRAY(void *, 2);
	data[0] = node;
	data[1] = &pan_time;
	/* Master morph */
	camera->pan_part = 0.0;
	morph_full( &camera->pan_part, MORPH_LINEAR, 1.0, pan_time, pan_step_cb, lpan_stage1_end_cb, data );

	/* Camera is under our control now */
	camera->manual_control = FALSE;

	camera_currently_moving = TRUE;
}


/* Sends camera back to view the previously visited node */
void
camera_look_at_previous( void )
{
	GNode *prev_node;

	/* Can't backtrack if history is empty */
	if (globals.history == NULL)
		return;

	/* Get previously visited node */
	prev_node = (GNode *)globals.history->data;

	globals.history->data = NULL;
	camera_look_at( prev_node );
}


/* Enters/exits bird's-eye-view mode */
void
camera_birdseye_view( boolean going_up )
{
	union AnyCamera new_anycam;
	Camera *new_cam, *pre_cam;
	RTvec ext_c1;
	double pan_time = 0.0;

	new_cam = CAMERA(&new_anycam);
	pre_cam = CAMERA(&pre_birdseye_view_camera);

	/* Neutralize user interface */
	window_set_access( FALSE );

	/* Save current scrollbar states */
	adj_copy( &prev_x_scrollbar_adj, x_scrollbar_adj );
	adj_copy( &prev_y_scrollbar_adj, y_scrollbar_adj );

	/* Halt any ongoing camera pan */
	camera_pan_break( );

	/* Determine length of pan */
	switch (globals.fsv_mode) {
		case FSV_DISCV:
		pan_time = DISCV_CAMERA_MAX_PAN_TIME;
		break;

		case FSV_MAPV:
		pan_time = MAPV_CAMERA_MAX_PAN_TIME;
		break;

		case FSV_TREEV:
		pan_time = TREEV_CAMERA_MAX_PAN_TIME;
		break;

		SWITCH_FAIL
	}

	if (going_up) {
		/* Save current camera state */
		memcpy( pre_cam, camera, sizeof(union AnyCamera) );

		/* Build bird's-eye view */
		new_cam->phi = 90.0;
		switch (globals.fsv_mode) {
			case FSV_DISCV:
			new_cam->distance = 2.0 * field_distance( camera->fov, 2.0 * DISCV_GEOM_PARAMS(root_dnode)->radius );
			break;

			case FSV_MAPV:
			new_cam->theta = 270.0;
			new_cam->distance = field_distance( camera->fov, MAPV_NODE_WIDTH(root_dnode) );
			break;

			case FSV_TREEV:
			new_cam->theta = 90.0 - TREEV_CAMERA(camera)->target.theta;
			if (dirtree_entry_expanded( root_dnode )) {
				geometry_treev_get_extents( root_dnode, NULL, &ext_c1 );
				new_cam->distance = field_distance( camera->fov, 2.0 * ext_c1.r );
			}
                        else
				new_cam->distance = 4.0 * camera->distance;
			break;

			SWITCH_FAIL
		}
		new_cam->near_clip = NEAR_TO_DISTANCE_RATIO * new_cam->distance;
		new_cam->far_clip = FAR_TO_NEAR_RATIO * new_cam->near_clip;

		morph( &camera->theta, MORPH_SIGMOID_ACCEL, new_cam->theta, pan_time );
		morph( &camera->phi, MORPH_SIGMOID_ACCEL, new_cam->phi, pan_time );
		morph( &camera->distance, MORPH_SIGMOID_ACCEL, new_cam->distance, pan_time );
		morph( &camera->near_clip, MORPH_SIGMOID_ACCEL, new_cam->near_clip, pan_time );
		morph( &camera->far_clip, MORPH_SIGMOID_ACCEL, new_cam->far_clip, pan_time );

		birdseye_view_active = TRUE;
	}
	else {
		/* Restore pre-bird's-eye-view camera state */
		morph( &camera->theta, MORPH_SIGMOID, pre_cam->theta, pan_time );
		morph( &camera->phi, MORPH_SIGMOID, pre_cam->phi, pan_time );
		morph( &camera->distance, MORPH_SIGMOID, pre_cam->distance, pan_time );
		morph( &camera->near_clip, MORPH_SIGMOID, pre_cam->near_clip, pan_time );
		morph( &camera->far_clip, MORPH_SIGMOID, pre_cam->far_clip, pan_time );

		switch (globals.fsv_mode) {
			case FSV_DISCV:
			morph( &DISCV_CAMERA(camera)->target.x, MORPH_SIGMOID, DISCV_CAMERA(pre_cam)->target.x, pan_time );
			morph( &DISCV_CAMERA(camera)->target.y, MORPH_SIGMOID, DISCV_CAMERA(pre_cam)->target.y, pan_time );
			break;

			case FSV_MAPV:
			morph( &MAPV_CAMERA(camera)->target.x, MORPH_SIGMOID, MAPV_CAMERA(pre_cam)->target.x, pan_time );
			morph( &MAPV_CAMERA(camera)->target.y, MORPH_SIGMOID, MAPV_CAMERA(pre_cam)->target.y, pan_time );
			morph( &MAPV_CAMERA(camera)->target.z, MORPH_SIGMOID, MAPV_CAMERA(pre_cam)->target.z, pan_time );
			break;

			case FSV_TREEV:
			morph( &TREEV_CAMERA(camera)->target.r, MORPH_SIGMOID, TREEV_CAMERA(pre_cam)->target.r, pan_time );
			morph( &TREEV_CAMERA(camera)->target.theta, MORPH_SIGMOID, TREEV_CAMERA(pre_cam)->target.theta, pan_time );
			morph( &TREEV_CAMERA(camera)->target.z, MORPH_SIGMOID, TREEV_CAMERA(pre_cam)->target.z, pan_time );
			break;

			SWITCH_FAIL
		}

		birdseye_view_active = FALSE;
	}

	/* Master morph */
	camera->pan_part = 0.0;
	morph_full( &camera->pan_part, MORPH_LINEAR, 1.0, pan_time, pan_step_cb, pan_end_cb, NULL );

	camera_currently_moving = TRUE;
}


/* Moves camera toward (dk < 0) or away (dk > 0) from view target */
void
camera_dolly( double dk )
{
	camera->distance += (dk * camera->distance / 256.0);
	camera->distance = MAX(camera->distance, 16.0);
	camera->near_clip = NEAR_TO_DISTANCE_RATIO * camera->distance;
	camera->far_clip = FAR_TO_NEAR_RATIO * camera->near_clip;

	/* Camera is under user control */
	camera->manual_control = TRUE;

	camera_update_scrollbars( TRUE );
	redraw( );
}


/* Revolves camera around view target by the given angle deltas */
void
camera_revolve( double dtheta, double dphi )
{
	/* theta = heading, phi = elevation */
	camera->theta -= dtheta;
	camera->phi += dphi;

	/* Keep angles within proper bounds */
	while (camera->theta < 0.0)
		camera->theta += 360.0;
	while (camera->theta > 360.0)
		camera->theta -= 360.0;
	camera->phi = CLAMP(camera->phi, 1.0, 90.0);

	/* Camera is under user control */
	camera->manual_control = TRUE;

	camera_update_scrollbars( TRUE );
	redraw( );
}


/* end camera.c */
