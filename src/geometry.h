/* geometry.h */

/* 3D geometry generation and rendering */

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


#ifdef FSV_GEOMETRY_H
	#error
#endif
#define FSV_GEOMETRY_H


/* Exported geometry constants */
#define TREEV_LEAF_NODE_EDGE		256.0
#define TREEV_PLATFORM_SPACING_DEPTH	2048.0

#define DISCV_GEOM_PARAMS(node)		((DiscVGeomParams *)(NODE_DESC(node)->geomparams))
#define MAPV_GEOM_PARAMS(node)		((MapVGeomParams *)(NODE_DESC(node)->geomparams))
#define TREEV_GEOM_PARAMS(node)		((TreeVGeomParams *)(NODE_DESC(node)->geomparams))

#define MAPV_NODE_WIDTH(node)		(MAPV_GEOM_PARAMS(node)->c1.x - MAPV_GEOM_PARAMS(node)->c0.x)
#define MAPV_NODE_DEPTH(node)		(MAPV_GEOM_PARAMS(node)->c1.y - MAPV_GEOM_PARAMS(node)->c0.y)
#define MAPV_NODE_CENTER_X(node)	(0.5 * (MAPV_GEOM_PARAMS(node)->c0.x + MAPV_GEOM_PARAMS(node)->c1.x))
#define MAPV_NODE_CENTER_Y(node)	(0.5 * (MAPV_GEOM_PARAMS(node)->c0.y + MAPV_GEOM_PARAMS(node)->c1.y))


/* Geometry parameters for a node in DiscV mode */
typedef struct _DiscVGeomParams DiscVGeomParams;
struct _DiscVGeomParams {
	/* WORK IN PROGRESS */
	double	radius;	/* Radius of node disc */
	double	theta;	/* Angle position on parent disc */
	XYvec	pos;	/* Center of disc w.r.t. center of parent (derived) */
};

/* Geometry parameters for a node in MapV mode */
typedef struct _MapVGeomParams MapVGeomParams;
struct _MapVGeomParams {
	XYvec	c0;	/* 2D left/front corner (x0,y0) */
	XYvec	c1;	/* 2D right/rear corner (x1,y1) */
	/* Note: x1 > x0 and y1 > y0 */
	double	height;	/* Height of node (bottom to top) */
};

/* Geometry parameters for a node in TreeV mode */
typedef struct _TreeVGeomParams TreeVGeomParams;
struct _TreeVGeomParams {
	struct {
		/* Distance from center of leaf to inner edge of parent */
		double distance;
		/* Angular position, relative to parent's centerline */
		double theta;
		/* Height of leaf (measured from bottom to top, not from z=0) */
		double height;
	} leaf;

	/* This next set is for expanded directories (platforms) only */
	struct {
		/* Angular position of centerline, relative to centerline
		 * of the parent directory */
		double theta;
		/* Distance from inner to outer edge */
		double depth;
		/* Arc width in degrees. This includes the constant-width
		 * spacer regions at either side of the platform */
		double arc_width;
		/* Height of platform (measured from z=0 to top) */
		double height;
		/* Overall arc width of subtree */
		double subtree_arc_width;
	} platform;
};


XYvec *geometry_discv_node_pos( GNode *node );
double geometry_mapv_node_z0( GNode *node );
double geometry_mapv_max_expanded_height( GNode *dnode );
boolean geometry_treev_is_leaf( GNode *node );
double geometry_treev_platform_r0( GNode *dnode );
double geometry_treev_platform_theta( GNode *dnode );
double geometry_treev_max_leaf_height( GNode *dnode );
void geometry_treev_get_extents( GNode *dnode, RTvec *ext_c0, RTvec *ext_c1 );
void geometry_queue_rebuild( GNode *dnode );
void geometry_init( FsvMode mode );
void geometry_gldraw_fsv( void );
void geometry_draw( boolean high_detail );
void geometry_camera_pan_finished( void );
void geometry_colexp_initiated( GNode *dnode );
void geometry_colexp_in_progress( GNode *dnode );
boolean geometry_should_highlight( GNode *node, unsigned int face_id );
void geometry_highlight_node( GNode *node, boolean strong );
void geometry_free_recursive( GNode *dnode );


/* end geometry.h */
