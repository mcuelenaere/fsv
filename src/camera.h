/* camera.h */

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


#ifdef FSV_CAMERA_H
	#error
#endif
#define FSV_CAMERA_H


/* Standard near-clip-to-camera and far-to-near-clip distance ratios */
#define NEAR_TO_DISTANCE_RATIO	0.5
#define FAR_TO_NEAR_RATIO	128.0


/* Camera type casts */
#define CAMERA(cam)		((Camera *)(cam))
#define DISCV_CAMERA(cam)	((DiscVCamera *)(cam))
#define MAPV_CAMERA(cam)	((MapVCamera *)(cam))
#define TREEV_CAMERA(cam)	((TreeVCamera *)(cam))


/* Base camera definition */
typedef struct _Camera Camera;
struct _Camera {
	double	theta;		/* Heading */
	double	phi;		/* Elevation */
	double	distance;	/* Distance between camera and target */
	double	fov;		/* Field of view, in degrees */
	double	near_clip;	/* Clipping plane distances */
	double	far_clip;
	double	pan_part;	/* Camera pan fraction (always in [0, 1]) */
	boolean	manual_control;	/* TRUE when camera is under manual control */
};

/* DiscV mode camera */
typedef struct _DiscVCamera DiscVCamera;
struct _DiscVCamera {
	Camera	camera;
	XYvec	target;
};

/* MapV mode camera */
typedef struct _MapVCamera MapVCamera;
struct _MapVCamera {
	Camera	camera;
	XYZvec	target;
};

/* TreeV mode camera */
typedef struct _TreeVCamera TreeVCamera;
struct _TreeVCamera {
	Camera	camera;
	RTZvec	target;
};

/* Generalized camera type */
union AnyCamera {
	Camera		camera;
	DiscVCamera	discv_camera;
	MapVCamera	mapv_camera;
	TreeVCamera	treev_camera;
};


/* The camera */
extern Camera *camera;


boolean camera_moving( void );
void camera_init( FsvMode mode, boolean initial_view );
#ifdef __GTK_H__
void camera_pass_scrollbar_widgets( GtkWidget *x_scrollbar_w, GtkWidget *y_scrollbar_w );
#endif
void camera_update_scrollbars( boolean hard_update );
void camera_pan_finish( void );
void camera_pan_break( void );
#ifdef FSV_ANIMATION_H
void camera_look_at_full( GNode *node, MorphType mtype, double pan_time_override );
#endif
void camera_look_at( GNode *node );
void camera_treev_lpan_look_at( GNode *node, double pan_time_override );
void camera_look_at_previous( void );
void camera_birdseye_view( boolean going_up );
void camera_dolly( double dk );
void camera_revolve( double dtheta, double dphi );


/* end camera.h */
