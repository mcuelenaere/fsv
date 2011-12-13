/* geometry.c */

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


#include "common.h"
#include "geometry.h"

#include <GL/gl.h>

#include "about.h"
#include "animation.h"
#include "camera.h"
#include "color.h"
#include "dirtree.h" /* dirtree_entry_expanded( ) */
#include "ogl.h"
#include "tmaptext.h"

/* 3D geometry for splash screen */
#include "fsv3d.h"


/* Cursor position remapping from linear to quarter-sine
 * (input and output are both [0, 1]) */
#define CURSOR_POS(x)			sin( (0.5 * PI) * (x) )

/* Use this to set the current GL color for a node */
#define node_glcolor( node ) \
	glColor3fv( (const float *)NODE_DESC(node)->color )


/* Low- and high-detail geometry display lists */
static GLuint fstree_low_dlist = NULL_DLIST;
static GLuint fstree_high_dlist = NULL_DLIST;

/* Current "drawing stage" for low- and high-detail geometry:
 * Stage 0: Full recursive draw, some geometry will be rebuilt along the way
 * Stage 1: Full recursive draw, no geometry rebuilt, capture everything in
 *          a display list (fstree_*_dlist, see above)
 * Stage 2: Fast draw using display list from stage 1 (no recursion) */
static int fstree_low_draw_stage;
static int fstree_high_draw_stage;


/* Forward declarations */
static void outline_pre( void );
static void outline_post( void );
static void cursor_pre( void );
static void cursor_hidden_part( void );
static void cursor_visible_part( void );
static void cursor_post( void );
static void queue_uncached_draw( void );


/**** DISC VISUALIZATION **************************************/


/* Geometry constants */
#define DISCV_CURVE_GRANULARITY		15.0
#define DISCV_LEAF_RANGE_ARC_WIDTH	315.0
#define DISCV_LEAF_STEM_PROPORTION	0.5

/* Messages for discv_draw_recursive( ) */
enum {
	DISCV_DRAW_GEOMETRY,
	DISCV_DRAW_LABELS
};


/* Returns the absolute position of the given node */
XYvec *
geometry_discv_node_pos( GNode *node )
{
	static XYvec pos;
	DiscVGeomParams *gparams;
	GNode *up_node;

	pos.x = 0.0;
	pos.y = 0.0;
	up_node = node;
	while (up_node != NULL) {
		gparams = DISCV_GEOM_PARAMS(up_node);
		pos.x += gparams->pos.x;
		pos.y += gparams->pos.y;
		up_node = up_node->parent;
	}

	return &pos;
}


/* Compare function for sorting nodes (by size) */
static int
discv_node_compare( GNode *a, GNode *b )
{
	int64 a_size, b_size;

	a_size = NODE_DESC(a)->size;
	if (NODE_IS_DIR(a))
		a_size += DIR_NODE_DESC(a)->subtree.size;

	b_size = NODE_DESC(b)->size;
	if (NODE_IS_DIR(b))
		b_size += DIR_NODE_DESC(b)->subtree.size;

	if (a_size < b_size)
		return 1;
	if (a_size > b_size)
		return -1;

	return strcmp( NODE_DESC(a)->name, NODE_DESC(b)->name );
}


/* Helper function for discv_init( ) */
static void
discv_init_recursive( GNode *dnode, double stem_theta )
{
	DiscVGeomParams *gparams;
	GNode *node;
	GList *node_list = NULL, *nl_llink;
	int64 node_size;
	double dir_radius, radius, dist;
	double arc_width, total_arc_width = 0.0;
	double theta0, theta1;
	double k;
	boolean even = TRUE;
	boolean stagger, out = TRUE;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (NODE_IS_DIR(dnode)) {
		morph_break( &DIR_NODE_DESC(dnode)->deployment );
		if (dirtree_entry_expanded( dnode ))
			DIR_NODE_DESC(dnode)->deployment = 1.0;
		else
			DIR_NODE_DESC(dnode)->deployment = 0.0;
		geometry_queue_rebuild( dnode );
	}

	/* If this directory has no children,
	 * there is nothing further to do here */
	if (dnode->children == NULL)
		return;

	dir_radius = DISCV_GEOM_PARAMS(dnode)->radius;

	/* Assign radii (and arc widths, temporarily) to leaf nodes */
	node = dnode->children;
	while (node != NULL) {
		node_size = MAX(64, NODE_DESC(node)->size);
                if (NODE_IS_DIR(node))
			node_size += DIR_NODE_DESC(node)->subtree.size;
		/* Area of disc == node_size */
		radius = sqrt( (double)node_size / PI );
		/* Center-to-center distance (parent to leaf) */
		dist = dir_radius + radius * (1.0 + DISCV_LEAF_STEM_PROPORTION);
		arc_width = 2.0 * DEG(asin( radius / dist ));
		gparams = DISCV_GEOM_PARAMS(node);
		gparams->radius = radius;
		gparams->theta = arc_width; /* temporary value */
		gparams->pos.x = dist; /* temporary value */
		total_arc_width += arc_width;
		node = node->next;
	}

	/* Create a list of leaf nodes, sorted by size */
	node = dnode->children;
	while (node != NULL) {
		G_LIST_PREPEND(node_list, node);
		node = node->next;
	}
	G_LIST_SORT(node_list, discv_node_compare);

	k = DISCV_LEAF_RANGE_ARC_WIDTH / total_arc_width;
	/* If this is going to be a tight fit, stagger the leaf nodes */
	stagger = k <= 1.0;

	/* Assign angle positions to leaf nodes, arranging them in clockwise
	 * order (spread out to occupy the entire available range), and
	 * recurse into subdirectories */
	theta0 = stem_theta - 180.0;
	theta1 = stem_theta + 180.0;
	nl_llink = node_list;
	while (nl_llink != NULL) {
		node = nl_llink->data;
		gparams = DISCV_GEOM_PARAMS(node);
		arc_width = k * gparams->theta;
		dist = gparams->pos.x;
		if (stagger && out) {
			/* Push leaf out */
			dist += 2.0 * gparams->radius;
		}
		if (nl_llink->prev == NULL) {
			/* First (largest) node */
			gparams->theta = theta0;
			theta0 += 0.5 * arc_width;
			theta1 -= 0.5 * arc_width;
			out = !out;
		}
		else if (even) {
			gparams->theta = theta0 + 0.5 * arc_width;
			theta0 += arc_width;
			out = !out;
		}
		else {
			gparams->theta = theta1 - 0.5 * arc_width;
			theta1 -= arc_width;
		}
		gparams->pos.x = dist * cos( RAD(gparams->theta) );
		gparams->pos.y = dist * sin( RAD(gparams->theta) );
		if (NODE_IS_DIR(node))
			discv_init_recursive( node, gparams->theta + 180.0 );
		even = !even;
		nl_llink = nl_llink->next;
	}

	g_list_free( node_list );
}


static void
discv_init( void )
{
	DiscVGeomParams *gparams;

	gparams = DISCV_GEOM_PARAMS(globals.fstree);
	gparams->radius = 0.0;
	gparams->theta = 0.0;

	discv_init_recursive( globals.fstree, 270.0 );

	gparams->pos.x = 0.0;
	gparams->pos.y = - DISCV_GEOM_PARAMS(root_dnode)->radius;

	/* DiscV mode is entirely 2D */
	glNormal3d( 0.0, 0.0, 1.0 );
}


/* Draws a DiscV node. dir_deployment is deployment of parent directory */
static void
discv_gldraw_node( GNode *node, double dir_deployment )
{
	static const int seg_count = (int)(360.0 / DISCV_CURVE_GRANULARITY + 0.999);
	DiscVGeomParams *gparams;
	XYvec center, p;
	double theta;
	int s;

	gparams = DISCV_GEOM_PARAMS(node);

	center.x = dir_deployment * gparams->pos.x;
	center.y = dir_deployment * gparams->pos.y;

	/* Draw disc */
	glBegin( GL_TRIANGLE_FAN );
	glVertex2d( center.x, center.y );
	for (s = 0; s <= seg_count; s++) {
		theta = (double)s / (double)seg_count * 360.0;
		p.x = center.x + gparams->radius * cos( RAD(theta) );
		p.y = center.y + gparams->radius * sin( RAD(theta) );
		glVertex2d( p.x, p.y );
	}
	glEnd( );
}


static void
discv_gldraw_folder( GNode *node )
{
	node = node;

	/* To be written... */

}


/* Builds the leaf nodes of a directory (but not the directory itself--
 * that geometry belongs to the parent) */
static void
discv_build_dir( GNode *dnode )
{
	GNode *node;
	double dpm;

	dpm = DIR_NODE_DESC(dnode)->deployment;
	/* TODO: Fix this, please */
	dpm = 1.0;

	node = dnode->children;
	while (node != NULL) {
		glLoadName( NODE_DESC(node)->id );
		node_glcolor( node );
		discv_gldraw_node( node, dpm );
		node = node->next;
	}
}


static void
discv_apply_label( GNode *node )
{
	node = node;

}


/* Helper function for discv_draw( ) */
static void
discv_draw_recursive( GNode *dnode, int action )
{
	DirNodeDesc *dir_ndesc;
	DiscVGeomParams *dir_gparams;
	GNode *node;
	boolean dir_collapsed;
	boolean dir_expanded;

	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_gparams = DISCV_GEOM_PARAMS(dnode);

	glPushMatrix( );

	dir_collapsed = DIR_COLLAPSED(dnode);
	dir_expanded = DIR_EXPANDED(dnode);

	glTranslated( dir_gparams->pos.x, dir_gparams->pos.y, 0.0 );
	glScaled( dir_ndesc->deployment,  dir_ndesc->deployment,  1.0 );

	if (action == DISCV_DRAW_GEOMETRY) {
		/* Draw folder or leaf nodes (display list A) */
		if (dir_ndesc->a_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->a_dlist == NULL_DLIST)
				dir_ndesc->a_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->a_dlist, GL_COMPILE_AND_EXECUTE );
			if (!dir_collapsed)
				discv_build_dir( dnode );
			if (!dir_expanded)
				discv_gldraw_folder( dnode );
			glEndList( );
			dir_ndesc->a_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->a_dlist );
	}

	if (action == DISCV_DRAW_LABELS) {
		/* Draw name label(s) (display list B) */
		if (dir_ndesc->b_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->b_dlist == NULL_DLIST)
				dir_ndesc->b_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->b_dlist, GL_COMPILE_AND_EXECUTE );
			/* Label leaf nodes */
			node = dnode->children;
			while (node != NULL) {
				discv_apply_label( node );
				node = node->next;
			}
			glEndList( );
			dir_ndesc->b_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->b_dlist );
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (dir_expanded) {
		/* Recurse into subdirectories */
		node = dnode->children;
		while (node != NULL) {
                        if (!NODE_IS_DIR(node))
				break;
			discv_draw_recursive( node, action );
			node = node->next;
		}
	}

	glPopMatrix( );
}


/* Draws DiscV geometry */
static void
discv_draw( boolean high_detail )
{
	glLineWidth( 3.0 );

	/* Draw low-detail geometry */

	if (fstree_low_draw_stage == 1)
		glNewList( fstree_low_dlist, GL_COMPILE_AND_EXECUTE );

	if (fstree_low_draw_stage <= 1)
		discv_draw_recursive( globals.fstree, DISCV_DRAW_GEOMETRY );
	else
		glCallList( fstree_low_dlist );

	if (fstree_low_draw_stage == 1)
		glEndList( );
	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;


	if (high_detail) {
		/* Draw additional high-detail geometry */

		if (fstree_high_draw_stage == 1)
			glNewList( fstree_high_dlist, GL_COMPILE_AND_EXECUTE );

		if (fstree_high_draw_stage <= 1) {
			/* Node name labels */
			text_pre( );
			glColor3f( 0.0, 0.0, 0.0 );
			discv_draw_recursive( globals.fstree, DISCV_DRAW_LABELS );
			text_post( );
		}
		else
			glCallList( fstree_high_dlist );

		if (fstree_high_draw_stage == 1)
			glEndList( );
		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		/* draw_cursor( ); */
	}

	glLineWidth( 1.0 );
}


static void
discv_gldraw_cursor( XYvec pos, double radius )
{
	pos.x = pos.x;
	radius = radius;

}


static void
discv_draw_cursor_between( GNode *a_node, GNode *b_node, double k )
{
	a_node = a_node;
	b_node = b_node;
	k = k;

}


/**** MAP VISUALIZATION ***************************************/


/* Geometry constants */
#define MAPV_BORDER_PROPORTION	0.01
#define MAPV_ROOT_ASPECT_RATIO	1.2

/* Messages for mapv_draw_recursive( ) */
enum {
	MAPV_DRAW_GEOMETRY,
	MAPV_DRAW_LABELS
};


/* Node side face offset ratios, by node type
 * (these define the obliqueness of a node's side faces) */
static const float mapv_side_slant_ratios[NUM_NODE_TYPES] = {
	NIL,	/* Metanode (not used) */
	0.032,	/* Directory */
	0.064,	/* Regular file */
	0.333,	/* Symlink */
	0.0,	/* FIFO */
	0.0,	/* Socket */
	0.25,	/* Character device */
	0.25,	/* Block device */
	0.0	/* Unknown */
};

/* Heights of directory and leaf nodes */
static double mapv_dir_height = 384.0;
static double mapv_leaf_height = 128.0;

/* Previous steady-state positions of the cursor corners
 * (if the cursor is moving from A to B, these delineate A) */
static XYZvec mapv_cursor_prev_c0;
static XYZvec mapv_cursor_prev_c1;


/* Returns the z-position of the bottom of a node */
double
geometry_mapv_node_z0( GNode *node )
{
	GNode *up_node;
	double z = 0.0;

	up_node = node->parent;
	while (up_node != NULL) {
		z += MAPV_GEOM_PARAMS(up_node)->height;
		up_node = up_node->parent;
	}

	return z;
}


/* Returns the peak height of a directory's contents (measured relative
 * to its top face), dictated by its expansion state as indicated by the
 * directory tree */
double
geometry_mapv_max_expanded_height( GNode *dnode )
{
	GNode *node;
	double height, max_height = 0.0;

	g_assert( NODE_IS_DIR(dnode) );

	if (dirtree_entry_expanded( dnode )) {
		node = dnode->children;
		while (node != NULL) {
			height = MAPV_GEOM_PARAMS(node)->height;
			if (NODE_IS_DIR(node)) {
				height += geometry_mapv_max_expanded_height( node );
				max_height = MAX(max_height, height);
			}
			else {
				max_height = MAX(max_height, height);
				break;
			}
			node = node->next;
		}
	}

	return max_height;
}


/* Helper function for mapv_init( ).
 * This is, in essence, the MapV layout engine */
static void
mapv_init_recursive( GNode *dnode )
{
	struct MapVBlock {
		GNode *node;
		double area;
	} *block, *next_first_block;
	struct MapVRow {
		struct MapVBlock *first_block;
		double area;
	} *row = NULL;
	MapVGeomParams *gparams;
	GNode *node;
	GList *block_list = NULL, *block_llink;
	GList *row_list = NULL, *row_llink;
	XYvec dir_dims, block_dims;
	XYvec start_pos, pos;
	double area, dir_area, total_block_area = 0.0;
	double nominal_border, border;
	double scale_factor;
	double a, b, k;
	int64 size;

	g_assert( NODE_IS_DIR(dnode) );

	morph_break( &DIR_NODE_DESC(dnode)->deployment );
	if (dirtree_entry_expanded( dnode ))
		DIR_NODE_DESC(dnode)->deployment = 1.0;
	else
		DIR_NODE_DESC(dnode)->deployment = 0.0;
	geometry_queue_rebuild( dnode );

	/* If this directory has no children,
	 * there is nothing further to do here */
	if (dnode->children == NULL)
		return;

	/* Obtain dimensions of top face of directory */
	dir_dims.x = MAPV_NODE_WIDTH(dnode);
	dir_dims.y = MAPV_NODE_DEPTH(dnode);
	k = mapv_side_slant_ratios[NODE_DIRECTORY];
	dir_dims.x -= 2.0 * MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dir_dims.x);
	dir_dims.y -= 2.0 * MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dir_dims.y);

	/* Approximate/nominal node border width (nodes will be spaced
	 * apart at about twice this distance) */
	a = MAPV_BORDER_PROPORTION * sqrt( dir_dims.x * dir_dims.y );
	b = MIN(dir_dims.x, dir_dims.y) / 3.0;
	nominal_border = MIN(a, b);

	/* Trim half a border width off the perimeter of the directory,
	 * so that nodes aren't laid down too close to the edges */
	dir_dims.x -= nominal_border;
	dir_dims.y -= nominal_border;
	dir_area = dir_dims.x * dir_dims.y;

	/* First pass
	 * 1. Create blocks. (A block is equivalent to a node, except
	 *    that it includes the node's surrounding border area)
	 * 2. Find total area of the blocks
	 * 3. Create a list of the blocks */
	node = dnode->children;
	while (node != NULL) {
		size = MAX(256, NODE_DESC(node)->size);
		if (NODE_IS_DIR(node))
			size += DIR_NODE_DESC(node)->subtree.size;
		k = sqrt( (double)size ) + nominal_border;
		area = SQR(k);
		total_block_area += area;

		block = NEW(struct MapVBlock);
		block->node = node;
		block->area = area;
		G_LIST_APPEND(block_list, block);

		node = node->next;
	}

	/* The blocks are going to have a total area greater than the
	 * directory can provide, so they'll have to be scaled down */
	scale_factor = dir_area / total_block_area;

	/* Second pass
	 * 1. Scale down the blocks
	 * 2. Generate a first-draft set of rows */
	block_llink = block_list;
	while (block_llink != NULL) {
		block = (struct MapVBlock *)block_llink->data;
		block->area *= scale_factor;

		if (row == NULL) {
			/* Begin new row */
			row = NEW(struct MapVRow);
			row->first_block = block;
			row->area = 0.0;
			G_LIST_APPEND(row_list, row);
		}

		/* Add block to row */
		row->area += block->area;

		/* Dimensions of block (block_dims.y == depth of row) */
		block_dims.y = row->area / dir_dims.x;
		block_dims.x = block->area / block_dims.y;

		/* Check aspect ratio of block */
		if ((block_dims.x / block_dims.y) < 1.0) {
			/* Next block will go into next row */
			row = NULL;
		}

		block_llink = block_llink->next;
	}

	/* Third pass - optimize layout */
	/* Note to self: write layout optimization routine sometime */

	/* Fourth pass - output final arrangement
	 * Start at right/rear corner, laying out rows of (mostly)
	 * successively smaller blocks */
	start_pos.x = MAPV_NODE_CENTER_X(dnode) + 0.5 * dir_dims.x;
	start_pos.y = MAPV_NODE_CENTER_Y(dnode) + 0.5 * dir_dims.y;
	pos.y = start_pos.y;
	block_llink = block_list;
	row_llink = row_list;
	while (row_llink != NULL) {
		row = (struct MapVRow *)row_llink->data;
		block_dims.y = row->area / dir_dims.x;
		pos.x = start_pos.x;

		/* Note first block of next row */
		if (row_llink->next == NULL)
			next_first_block = NULL;
		else
			next_first_block = ((struct MapVRow *)row_llink->next->data)->first_block;

		/* Output one row */
		while (block_llink != NULL) {
			block = (struct MapVBlock *)block_llink->data;
			if (block == next_first_block)
				break; /* finished with row */
			block_dims.x = block->area / block_dims.y;

			size = MAX(256, NODE_DESC(block->node)->size);
			if (NODE_IS_DIR(block->node))
				size += DIR_NODE_DESC(block->node)->subtree.size;
			area = scale_factor * (double)size;

			/* Calculate exact width of block's border region */
			k = block_dims.x + block_dims.y;
			/* Note: area == scaled area of node,
			 * block->area == scaled area of node + border */
			border = 0.25 * (k - sqrt( SQR(k) - 4.0 * (block->area - area) ));

			/* Assign geometry
			 * (Note: pos is right/rear corner of block) */
			gparams = MAPV_GEOM_PARAMS(block->node);
			gparams->c0.x = pos.x - block_dims.x + border;
			gparams->c0.y = pos.y - block_dims.y + border;
			gparams->c1.x = pos.x - border;
			gparams->c1.y = pos.y - border;

			if (NODE_IS_DIR(block->node)) {
				gparams->height = mapv_dir_height;

				/* Recurse into directory */
				mapv_init_recursive( block->node );
			}
			else
				gparams->height = mapv_leaf_height;

			pos.x -= block_dims.x;
			block_llink = block_llink->next;
		}

		pos.y -= block_dims.y;
		row_llink = row_llink->next;
	}

	/* Clean up */

	block_llink = block_list;
	while (block_llink != NULL) {
		xfree( block_llink->data );
		block_llink = block_llink->next;
	}
	g_list_free( block_list );

	row_llink = row_list;
	while (row_llink != NULL) {
		xfree( row_llink->data );
		row_llink = row_llink->next;
	}
	g_list_free( row_list );
}


/* Top-level call to initialize MapV mode */
static void
mapv_init( void )
{
	MapVGeomParams *gparams;
	XYvec root_dims;
	double k;

	/* Determine dimensions of bottommost (root) node */
	root_dims.y = sqrt( (double)DIR_NODE_DESC(globals.fstree)->subtree.size / MAPV_ROOT_ASPECT_RATIO );
	root_dims.x = MAPV_ROOT_ASPECT_RATIO * root_dims.y;

	/* Set up base geometry */
	MAPV_GEOM_PARAMS(globals.fstree)->height = 0.0;
	gparams = MAPV_GEOM_PARAMS(root_dnode);
	gparams->c0.x = -0.5 * root_dims.x;
	gparams->c0.y = -0.5 * root_dims.y;
	gparams->c1.x = 0.5 * root_dims.x;
	gparams->c1.y = 0.5 * root_dims.y;
	gparams->height = mapv_dir_height;

	mapv_init_recursive( root_dnode );

	/* Initial cursor state */
	if (globals.current_node == root_dnode)
		k = 4.0;
	else
		k = 1.25;
	mapv_cursor_prev_c0.x = k * MAPV_GEOM_PARAMS(root_dnode)->c0.x;
	mapv_cursor_prev_c0.y = k * MAPV_GEOM_PARAMS(root_dnode)->c0.y;
	mapv_cursor_prev_c0.z = - 0.25 * k * MAPV_NODE_DEPTH(root_dnode);
	mapv_cursor_prev_c1.x = k * MAPV_GEOM_PARAMS(root_dnode)->c1.x;
	mapv_cursor_prev_c1.y = k * MAPV_GEOM_PARAMS(root_dnode)->c1.y;
	mapv_cursor_prev_c1.z = 0.25 * k * MAPV_NODE_DEPTH(root_dnode);
}


/* Hook function for camera pan completion */
static void
mapv_camera_pan_finished( void )
{
	/* Save cursor position */
	mapv_cursor_prev_c0.x = MAPV_GEOM_PARAMS(globals.current_node)->c0.x;
	mapv_cursor_prev_c0.y = MAPV_GEOM_PARAMS(globals.current_node)->c0.y;
	mapv_cursor_prev_c0.z = geometry_mapv_node_z0( globals.current_node );
	mapv_cursor_prev_c1.x = MAPV_GEOM_PARAMS(globals.current_node)->c1.x;
	mapv_cursor_prev_c1.y = MAPV_GEOM_PARAMS(globals.current_node)->c1.y;
	mapv_cursor_prev_c1.z = mapv_cursor_prev_c0.z + MAPV_GEOM_PARAMS(globals.current_node)->height;
}


/* Draws a MapV node */
static void
mapv_gldraw_node( GNode *node )
{
	MapVGeomParams *gparams;
	XYZvec dims;
	XYvec offset, normal;
	double normal_z_nx, normal_z_ny;
	double a, b, k;

	/* Dimensions of node */
	dims.x = MAPV_NODE_WIDTH(node);
	dims.y = MAPV_NODE_DEPTH(node);
	dims.z = MAPV_GEOM_PARAMS(node)->height;

	/* Calculate normals for slanted sides */
	k = mapv_side_slant_ratios[NODE_DESC(node)->type];
	offset.x = MIN(dims.z, k * dims.x);
	offset.y = MIN(dims.z, k * dims.y);
	a = sqrt( SQR(offset.x) + SQR(dims.z) );
	b = sqrt( SQR(offset.y) + SQR(dims.z) );
	normal.x = dims.z / a;
	normal.y = dims.z / b;
	normal_z_nx = offset.x / a;
	normal_z_ny = offset.y / b;

	gparams = MAPV_GEOM_PARAMS(node);

	/* Draw sides of node */
	glBegin( GL_QUAD_STRIP );
	glNormal3d( 0.0, normal.y, normal_z_ny ); /* Rear face */
	glVertex3d( gparams->c0.x, gparams->c1.y, 0.0 );
	glVertex3d( gparams->c0.x + offset.x, gparams->c1.y - offset.y, gparams->height );
	glNormal3d( normal.x, 0.0, normal_z_nx ); /* Right face */
	glVertex3d( gparams->c1.x, gparams->c1.y, 0.0 );
	glVertex3d( gparams->c1.x - offset.x, gparams->c1.y - offset.y, gparams->height );
	glNormal3d( 0.0, - normal.y, normal_z_ny ); /* Front face */
	glVertex3d( gparams->c1.x, gparams->c0.y, 0.0 );
	glVertex3d( gparams->c1.x - offset.x, gparams->c0.y + offset.y, gparams->height );
	glNormal3d( - normal.x, 0.0, normal_z_nx ); /* Left face */
	glVertex3d( gparams->c0.x, gparams->c0.y, 0.0 );
	glVertex3d( gparams->c0.x + offset.x, gparams->c0.y + offset.y, gparams->height );
	glVertex3d( gparams->c0.x, gparams->c1.y, 0.0 ); /* Close strip */
	glVertex3d( gparams->c0.x + offset.x, gparams->c1.y - offset.y, gparams->height );
	glEnd( );

	/* Top face has ID of 1 */
	glPushName( 1 );

	/* Draw top face */
	glNormal3d( 0.0, 0.0, 1.0 );
	glBegin( GL_QUADS );
	glVertex3d( gparams->c0.x + offset.x, gparams->c0.y + offset.y, gparams->height );
	glVertex3d( gparams->c1.x - offset.x, gparams->c0.y + offset.y, gparams->height );
	glVertex3d( gparams->c1.x - offset.x, gparams->c1.y - offset.y, gparams->height );
	glVertex3d( gparams->c0.x + offset.x, gparams->c1.y - offset.y, gparams->height );
	glEnd( );

	glPopName( );
}


/* Draws a "folder" shape atop a directory */
static void
mapv_gldraw_folder( GNode *dnode )
{
	XYvec dims, offset;
	XYvec c0, c1;
	XYvec folder_c0, folder_c1, folder_tab;
	double k, border;

	g_assert( NODE_IS_DIR(dnode) );

	/* Obtain corners/dimensions of top face */
	dims.x = MAPV_NODE_WIDTH(dnode);
	dims.y = MAPV_NODE_DEPTH(dnode);
	k = mapv_side_slant_ratios[NODE_DIRECTORY];
	offset.x = MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dims.x);
	offset.y = MIN(MAPV_GEOM_PARAMS(dnode)->height, k * dims.y);
	c0.x = MAPV_GEOM_PARAMS(dnode)->c0.x + offset.x;
	c0.y = MAPV_GEOM_PARAMS(dnode)->c0.y + offset.y;
	c1.x = MAPV_GEOM_PARAMS(dnode)->c1.x - offset.x;
	c1.y = MAPV_GEOM_PARAMS(dnode)->c1.y - offset.y;
	dims.x -= 2.0 * offset.x;
	dims.y -= 2.0 * offset.y;

	/* Folder geometry */
	border = 0.0625 * MIN(dims.x, dims.y);
	folder_c0.x = c0.x + border;
	folder_c0.y = c0.y + border;
	folder_c1.x = c1.x - border;
	folder_c1.y = c1.y - border;
	/* Coordinates of the concave vertex */
	folder_tab.x = folder_c1.x - (MAGIC_NUMBER - 1.0) * (folder_c1.x - folder_c0.x);
	folder_tab.y = folder_c1.y - border;

	node_glcolor( dnode );
	glBegin( GL_LINE_STRIP );
	glVertex2d( folder_c0.x, folder_c0.y );
	glVertex2d( folder_c0.x, folder_tab.y );
	glVertex2d( folder_c0.x + border, folder_c1.y );
	glVertex2d( folder_tab.x - border, folder_c1.y );
	glVertex2d( folder_tab.x, folder_tab.y );
	glVertex2d( folder_c1.x, folder_tab.y );
	glVertex2d( folder_c1.x, folder_c0.y );
	glVertex2d( folder_c0.x, folder_c0.y );
	glEnd( );
}


/* Builds the children of a directory (but not the directory itself;
 * that geometry belongs to the parent) */
static void
mapv_build_dir( GNode *dnode )
{
	GNode *node;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	node = dnode->children;
	while (node != NULL) {
		/* Draw node */
		glLoadName( NODE_DESC(node)->id );
		node_glcolor( node );
		mapv_gldraw_node( node );
		node = node->next;
	}
}


/* Draws a node name label */
static void
mapv_apply_label( GNode *node )
{
	XYZvec label_pos;
	XYvec dims, label_dims;
	double k;

	/* Obtain dimensions of top face */
	dims.x = MAPV_NODE_WIDTH(node);
	dims.y = MAPV_NODE_DEPTH(node);
	k = mapv_side_slant_ratios[NODE_DESC(node)->type];
	dims.x -= 2.0 * MIN(MAPV_GEOM_PARAMS(node)->height, k * dims.x);
	dims.y -= 2.0 * MIN(MAPV_GEOM_PARAMS(node)->height, k * dims.y);

	/* (Maximum) dimensions of label */
	label_dims.x = 0.8125 * dims.x;
	label_dims.y = (2.0 - MAGIC_NUMBER) * dims.y;

	/* Center position of label */
	label_pos.x = MAPV_NODE_CENTER_X(node);
	label_pos.y = MAPV_NODE_CENTER_Y(node);
	if (NODE_IS_DIR(node))
		label_pos.z = 0.0;
	else
		label_pos.z = MAPV_GEOM_PARAMS(node)->height;

	text_draw_straight( NODE_DESC(node)->name, &label_pos, &label_dims );
}


/* MapV mode "full draw" */
static void
mapv_draw_recursive( GNode *dnode, int action )
{
	DirNodeDesc *dir_ndesc;
	GNode *node;
	boolean dir_collapsed;
	boolean dir_expanded;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	glPushMatrix( );
	glTranslated( 0.0, 0.0, MAPV_GEOM_PARAMS(dnode)->height );

	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_collapsed = DIR_COLLAPSED(dnode);
	dir_expanded = DIR_EXPANDED(dnode);

	if (!dir_collapsed && !dir_expanded) {
		/* Grow/shrink children heightwise */
		glEnable( GL_NORMALIZE );
		glScaled( 1.0, 1.0, dir_ndesc->deployment );
	}

	if (action == MAPV_DRAW_GEOMETRY) {
		/* Draw directory face or geometry of children
		 * (display list A) */
		if (dir_ndesc->a_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->a_dlist == NULL_DLIST)
				dir_ndesc->a_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->a_dlist, GL_COMPILE_AND_EXECUTE );
			if (dir_collapsed)
				mapv_gldraw_folder( dnode );
			else
				mapv_build_dir( dnode );
			glEndList( );
			dir_ndesc->a_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->a_dlist );
	}

	if (action == MAPV_DRAW_LABELS) {
		/* Draw name label(s) (display list B) */
		if (dir_ndesc->b_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->b_dlist == NULL_DLIST)
				dir_ndesc->b_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->b_dlist, GL_COMPILE_AND_EXECUTE );
			if (dir_collapsed) {
				/* Label directory */
				mapv_apply_label( dnode );
			}
			else {
				/* Label non-subdirectory children */
				node = dnode->children;
				while (node != NULL) {
					if (!NODE_IS_DIR(node))
						mapv_apply_label( node );
					node = node->next;
				}
			}
			glEndList( );
			dir_ndesc->b_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->b_dlist );
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (!dir_collapsed) {
		/* Recurse into subdirectories */
		node = dnode->children;
		while (node != NULL) {
			if (!NODE_IS_DIR(node))
				break;
			mapv_draw_recursive( node, action );
			node = node->next;
		}
	}

	if (!dir_collapsed && !dir_expanded)
		glDisable( GL_NORMALIZE );

	glPopMatrix( );
}


/* Draws the node cursor, size/position specified by corners */
static void
mapv_gldraw_cursor( const XYZvec *c0, const XYZvec *c1 )
{
	static const double bar_part = SQR(SQR(MAGIC_NUMBER - 1.0));
	XYZvec corner_dims;
	XYZvec p, delta;
	int i, c;

	corner_dims.x = bar_part * (c1->x - c0->x);
	corner_dims.y = bar_part * (c1->y - c0->y);
	corner_dims.z = bar_part * (c1->z - c0->z);

	cursor_pre( );
	for (i = 0; i < 2; i++) {
		if (i == 0)
			cursor_hidden_part( );
		else if (i == 1)
			cursor_visible_part( );

		glBegin( GL_LINES );
		for (c = 0; c < 8; c++) {
			if (c & 1) {
				p.x = c1->x;
				delta.x = - corner_dims.x;
			}
			else {
				p.x = c0->x;
				delta.x = corner_dims.x;
			}

			if (c & 2) {
				p.y = c1->y;
				delta.y = - corner_dims.y;
			}
			else {
				p.y = c0->y;
				delta.y = corner_dims.y;
			}

			if (c & 4) {
				p.z = c1->z;
				delta.z = - corner_dims.z;
			}
			else {
				p.z = c0->z;
				delta.z = corner_dims.z;
			}

			glVertex3d( p.x, p.y, p.z );
			glVertex3d( p.x + delta.x, p.y, p.z );

			glVertex3d( p.x, p.y, p.z );
			glVertex3d( p.x, p.y + delta.y, p.z );

			glVertex3d( p.x, p.y, p.z );
			glVertex3d( p.x, p.y, p.z + delta.z );
		}
		glEnd( );
	}
	cursor_post( );
}


/* Draws the node cursor in an intermediate position between its previous
 * steady-state position and the current node (pos=0 indicates the former,
 * pos=1 the latter) */
static void
mapv_draw_cursor( double pos )
{
	MapVGeomParams *gparams;
	XYZvec cursor_c0, cursor_c1;
	double z0;

	gparams = MAPV_GEOM_PARAMS(globals.current_node);
        z0 = geometry_mapv_node_z0( globals.current_node );

	/* Interpolate corners */
	cursor_c0.x = INTERPOLATE(pos, mapv_cursor_prev_c0.x, gparams->c0.x);
	cursor_c0.y = INTERPOLATE(pos, mapv_cursor_prev_c0.y, gparams->c0.y);
	cursor_c0.z = INTERPOLATE(pos, mapv_cursor_prev_c0.z, z0);
	cursor_c1.x = INTERPOLATE(pos, mapv_cursor_prev_c1.x, gparams->c1.x);
	cursor_c1.y = INTERPOLATE(pos, mapv_cursor_prev_c1.y, gparams->c1.y);
	cursor_c1.z = INTERPOLATE(pos, mapv_cursor_prev_c1.z, z0 + gparams->height);

	mapv_gldraw_cursor( &cursor_c0, &cursor_c1 );
}


/* Draws MapV geometry */
static void
mapv_draw( boolean high_detail )
{
	/* Draw low-detail geometry */

	if (fstree_low_draw_stage == 1)
		glNewList( fstree_low_dlist, GL_COMPILE_AND_EXECUTE );

	if (fstree_low_draw_stage <= 1)
		mapv_draw_recursive( globals.fstree, MAPV_DRAW_GEOMETRY );
	else
		glCallList( fstree_low_dlist );

	if (fstree_low_draw_stage == 1)
		glEndList( );
	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;

	if (high_detail) {
		/* Draw additional high-detail stuff */

		if (fstree_high_draw_stage == 1)
			glNewList( fstree_high_dlist, GL_COMPILE_AND_EXECUTE );

		if (fstree_high_draw_stage <= 1) {
			/* "Cel lines" */
			outline_pre( );
			if (fstree_low_draw_stage <= 1)
				mapv_draw_recursive( globals.fstree, MAPV_DRAW_GEOMETRY );
			else
				glCallList( fstree_low_dlist ); /* shortcut */
			outline_post( );

			/* Node name labels */
			text_pre( );
			glColor3f( 0.0, 0.0, 0.0 ); /* all labels are black */
			mapv_draw_recursive( globals.fstree, MAPV_DRAW_LABELS );
			text_post( );
		}
		else
			glCallList( fstree_high_dlist );

		if (fstree_high_draw_stage == 1)
			glEndList( );
		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		mapv_draw_cursor( CURSOR_POS(camera->pan_part) );
	}
}


/**** TREE VISUALIZATION **************************************/


/* Geometry constants */
#define TREEV_MIN_ARC_WIDTH		90.0
#define TREEV_MAX_ARC_WIDTH		225.0
#define TREEV_BRANCH_WIDTH		256.0
#define TREEV_MIN_CORE_RADIUS		8192.0
#define TREEV_CORE_GROW_FACTOR		1.25
#define TREEV_CURVE_GRANULARITY		5.0
#define TREEV_PLATFORM_HEIGHT		158.2
#define TREEV_PLATFORM_SPACING_WIDTH	512.0
#define TREEV_LEAF_HEIGHT_MULTIPLIER	1.0
#define TREEV_LEAF_PADDING		(0.125 * TREEV_LEAF_NODE_EDGE)
#define TREEV_PLATFORM_PADDING		(0.5 * TREEV_PLATFORM_SPACING_WIDTH)

/* Extra flags for TreeV mode */
enum {
	TREEV_NEED_REARRANGE	= 1 << 0
};

/* Messages for treev_draw_recursive( ) */
enum {
	/* Note: don't change order of these */
	TREEV_DRAW_LABELS,
	TREEV_DRAW_GEOMETRY,
	TREEV_DRAW_GEOMETRY_WITH_BRANCHES
};


/* Color of interconnecting branches */
static RGBcolor branch_color = { 0.5, 0.0, 0.0 };

/* Label colors for platform and leaf nodes */
static RGBcolor treev_platform_label_color = { 1.0, 1.0, 1.0 };
static RGBcolor treev_leaf_label_color = { 0.0, 0.0, 0.0 };

/* Point buffers used in drawing curved geometry */
static XYvec *inner_edge_buf = NULL;
static XYvec *outer_edge_buf = NULL;

/* Radius of innermost loop */
static double treev_core_radius;

/* Previous steady-state positions of the cursor corners */
static RTZvec treev_cursor_prev_c0;
static RTZvec treev_cursor_prev_c1;


/* Checks if a node is currently a leaf (i.e. a collapsed directory or some
 * other node), or not (an expanded directory) according to the directory
 * tree */
boolean
geometry_treev_is_leaf( GNode *node )
{
	if (NODE_IS_DIR(node))
		if (dirtree_entry_expanded( node ))
			return FALSE;

	return TRUE;
}


/* Returns the inner radius of a directory platform */
double
geometry_treev_platform_r0( GNode *dnode )
{
	GNode *up_node;
	double r0 = 0.0;

	if (NODE_IS_METANODE(dnode))
		return treev_core_radius;

	up_node = dnode->parent;
	while (up_node != NULL) {
		r0 += TREEV_PLATFORM_SPACING_DEPTH;
		r0 += TREEV_GEOM_PARAMS(up_node)->platform.depth;
		up_node = up_node->parent;
	}
	r0 += treev_core_radius;

	return r0;
}


/* Returns the absolute angular position of a directory platform (which
 * is referenced along the platform's radial centerline) */
double
geometry_treev_platform_theta( GNode *dnode )
{
	GNode *up_node;
	double theta = 0.0;

	g_assert( !geometry_treev_is_leaf( dnode ) || NODE_IS_METANODE(dnode) );

	up_node = dnode;
	while (up_node != NULL) {
		theta += TREEV_GEOM_PARAMS(up_node)->platform.theta;
		up_node = up_node->parent;
	}

	return theta;
}


/* This returns the height of the tallest leaf on the given directory
 * platform. Height does not include that of the platform itself */
double
geometry_treev_max_leaf_height( GNode *dnode )
{
	GNode *node;
	double max_height = 0.0;

	g_assert( !geometry_treev_is_leaf( dnode ) );

	node = dnode->children;
	while (node != NULL) {
		if (geometry_treev_is_leaf( node ))
			max_height = MAX(max_height, TREEV_GEOM_PARAMS(node)->leaf.height);
		node = node->next;
	}

	return max_height;
}


/* Helper function for treev_get_extents( ) */
static void
treev_get_extents_recursive( GNode *dnode, RTvec *c0, RTvec *c1, double r0, double theta )
{
	GNode *node;
	double subtree_r0;

	g_assert( NODE_IS_DIR(dnode) );

	subtree_r0 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
	node = dnode->children;
	while (node != NULL) {
		if (!geometry_treev_is_leaf( node ))
			treev_get_extents_recursive( node, c0, c1, subtree_r0, theta + TREEV_GEOM_PARAMS(node)->platform.theta );
/* TODO: try putting this check at top of loop */
		if (!NODE_IS_DIR(node))
			break;
		node = node->next;
	}

	c0->r = MIN(c0->r, r0);
	c0->theta = MIN(c0->theta, theta - TREEV_GEOM_PARAMS(dnode)->platform.arc_width);
	c1->r = MAX(c1->r, r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth);
	c1->theta = MAX(c1->theta, theta + TREEV_GEOM_PARAMS(dnode)->platform.arc_width);
}


/* This returns the 2D corners of the entire subtree rooted at the given
 * directory platform, including the subtree root. (Note that the extents
 * returned depend on the current expansion state) */
void
geometry_treev_get_extents( GNode *dnode, RTvec *ext_c0, RTvec *ext_c1 )
{
	RTvec c0, c1;

	g_assert( !geometry_treev_is_leaf( dnode ) );

	c0.r = DBL_MAX;
	c0.theta = DBL_MAX;
	c1.r = DBL_MIN;
	c1.theta = DBL_MIN;

	treev_get_extents_recursive( dnode, &c0, &c1, geometry_treev_platform_r0( dnode ), geometry_treev_platform_theta( dnode ) );

	if (ext_c0 != NULL)
		*ext_c0 = c0; /* struct assign */
	if (ext_c1 != NULL)
		*ext_c1 = c1; /* struct assign */
}


/* Returns the corners (min/max RTZ points) of the given leaf node or
 * directory platform in absolute polar coordinates, with some padding
 * added on all sides for a not-too-tight fit */
static void
treev_get_corners( GNode *node, RTZvec *c0, RTZvec *c1 )
{
	RTZvec pos;
	double leaf_arc_width;
	double padding_arc_width;

	if (geometry_treev_is_leaf( node )) {
		/* Absolute position of center of leaf node bottom */
		pos.r = geometry_treev_platform_r0( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.distance;
		pos.theta = geometry_treev_platform_theta( node->parent ) + TREEV_GEOM_PARAMS(node)->leaf.theta;
		pos.z = TREEV_GEOM_PARAMS(node->parent)->platform.height;

		/* Calculate corners of leaf node */
		leaf_arc_width = (180.0 * TREEV_LEAF_NODE_EDGE / PI) / pos.r;
		c0->r = pos.r - (0.5 * TREEV_LEAF_NODE_EDGE);
		c0->theta = pos.theta - 0.5 * leaf_arc_width;
		c0->z = pos.z;
		c1->r = pos.r + (0.5 * TREEV_LEAF_NODE_EDGE);
		c1->theta = pos.theta + 0.5 * leaf_arc_width;
		c1->z = pos.z + TREEV_GEOM_PARAMS(node)->leaf.height;

		/* Push corners outward a bit */
		padding_arc_width = (180.0 * TREEV_LEAF_PADDING / PI) / pos.r;
		c0->r -= TREEV_LEAF_PADDING;
		c0->theta -= padding_arc_width;
		c0->z -= (0.5 * TREEV_LEAF_PADDING);
		c1->r += TREEV_LEAF_PADDING;
		c1->theta += padding_arc_width;
		c1->z += (0.5 * TREEV_LEAF_PADDING);
	}
	else {
		/* Position of center of inner edge of platform */
		pos.r = geometry_treev_platform_r0( node );
		pos.theta = geometry_treev_platform_theta( node );

		/* Calculate corners of platform region */
		c0->r = pos.r;
		c0->theta = pos.theta - 0.5 * TREEV_GEOM_PARAMS(node)->platform.arc_width;
		c0->z = 0.0;
		c1->r = pos.r + TREEV_GEOM_PARAMS(node)->platform.depth;
		c1->theta = pos.theta + 0.5 * TREEV_GEOM_PARAMS(node)->platform.arc_width;
		c1->z = TREEV_GEOM_PARAMS(node)->platform.height;

		/* Push corners outward a bit. Because the sides already
		 * encompass the platform spacing regions, there is no need
                 * to add extra padding there */
		c0->r -= TREEV_PLATFORM_PADDING;
		c1->r += TREEV_PLATFORM_PADDING;
	}
}


/* This assigns an arc width and depth to a directory platform.
 * Note: depth value is only an estimate; the final value can only be
 * determined by actually laying down leaf nodes */
static void
treev_reshape_platform( GNode *dnode, double r0 )
{
#define edge05 (0.5 * TREEV_LEAF_NODE_EDGE)
#define edge15 (1.5 * TREEV_LEAF_NODE_EDGE)
	static const double w = TREEV_PLATFORM_SPACING_WIDTH;
	static const double w_2 = SQR(TREEV_PLATFORM_SPACING_WIDTH);
	static const double w_3 = SQR(TREEV_PLATFORM_SPACING_WIDTH) * TREEV_PLATFORM_SPACING_WIDTH;
	static const double w_4 = SQR(TREEV_PLATFORM_SPACING_WIDTH) * SQR(TREEV_PLATFORM_SPACING_WIDTH);
	double area;
	double A, A_2, A_3, r, r_2, r_3, r_4, ka, kb, kc, kd, d, theta;
	double depth, arc_width, min_arc_width;
	double k;
	int n;

	/* Estimated area, based on number of (immediate) children */
	n = g_list_length( (GList *)dnode->children );
	k = edge15 * ceil( sqrt( (double)MAX(1, n) ) ) + edge05;
	area = SQR(k);

	/* Known: Area and inner radius of directory, plus the fact that
	 * the aspect ratio (length_of_outer_edge / depth) is exactly 1.
	 * Unknown: depth and arc width of directory.
	 * Raw and distilled equations:
	 * { A ~= PI*theta/360*((r + d)^2 - r^2) - w*d,
	 * s ~= PI*theta*(r + d)/180 - w,
	 * s/d = 1  -->  s = d,
	 * theta = 180*(d + w)/(PI*(r + d)),
	 * d^3 + (2*r + w)*d^2 + (2*w*r - 2*A - w)*d - 2*A*r = 0,
	 * A = area, w = TREEV_PLATFORM_SPACING_WIDTH, r = r0,
	 * s = (length of outer edge), d = depth, theta = arc_width }
	 * Solution: Thank god for Maple */
	A = area;
	A_2 = SQR(A);
	A_3 = A*A_2;
	r = r0;
	r_2 = SQR(r);
	r_3 = r*r_2;
	r_4 = SQR(r_2);
	ka = 72.0*(A*r - w*(A + r)) - 64.0*r_3 + 48.0*r_2*w - 36.0*w_2 + 24.0*r*w_2 - 8.0*w_3;
#define T1 72.0*A*w_2 - 132.0*A*r*w_2 - 240.0*A*w*r_3 + 120.0*A*w_2*r_2 - 24.0*A_2*w*r - 60.0*w_3*r
#define T2 12.0*(w_2*r_2 + A_2*w_2 - w_4*r + w_4*r_2 + A*w_3 + w_3)
#define T3 48.0*(w_2*r_4 - w_2*r_3 - w_3*r_3) + 96.0*(A_3 + w_3*r_2)
#define T4 192.0*A*r_4 + 156.0*A_2*r_2 + 3.0*w_4 + 144.0*A_2*w + 264.0*A*w*r_2
	kb = 12.0*sqrt( T1 + T2 + T3 + T4 );
#undef T1
#undef T2
#undef T3
#undef T4
	kc = cos( atan2( kb, ka ) / 3.0 );
	kd = cbrt( hypot( ka, kb ) );
	/* Bring it all together */
	d = (- w - 2.0*r)/3.0 + ((8.0*r_2 - 4.0*w*r + 2.0*w_2)/3.0 + 4.0*A + 2.0*w)*kc/kd + kc*kd/6.0;
	theta = 180.0*(d + w)/(PI*(r + d));

	depth = d;
	arc_width = theta;

	/* Adjust depth upward to accomodate an integral number of rows */
	depth += (edge15 - fmod( depth - edge05, edge15 )) + edge05;

	/* Final arc width must be at least large enough to yield an
	 * inner edge length that is two leaf node edges long */
	min_arc_width = (180.0 * (2.0 * TREEV_LEAF_NODE_EDGE + TREEV_PLATFORM_SPACING_WIDTH) / PI) / r0;

	TREEV_GEOM_PARAMS(dnode)->platform.arc_width = MAX(min_arc_width, arc_width);
	TREEV_GEOM_PARAMS(dnode)->platform.depth = depth;

	/* Directory will need rebuilding, obviously */
	geometry_queue_rebuild( dnode );

#undef edge05
#undef edge15
}


/* Helper function for treev_arrange( ). @reshape_tree flag should be TRUE
 * if platform radiuses have changed (thus requiring reshaping) */
static void
treev_arrange_recursive( GNode *dnode, double r0, boolean reshape_tree )
{
	GNode *node;
	double subtree_r0;
	double arc_width, subtree_arc_width = 0.0;
	double theta;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (!reshape_tree && !(NODE_DESC(dnode)->flags & TREEV_NEED_REARRANGE))
		return;

	if (reshape_tree && NODE_IS_DIR(dnode)) {
		if (geometry_treev_is_leaf(dnode)) {
			/* Ensure directory leaf gets repositioned */
			geometry_queue_rebuild( dnode );
			return;
		}
		else {
			/* Reshape directory platform */
			treev_reshape_platform( dnode, r0 );
		}
	}

	/* Recurse into expanded subdirectories, and obtain the overall
	 * arc width of the subtree */
	subtree_r0 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
	node = dnode->children;
	while (node != NULL) {
		if (!NODE_IS_DIR(node))
			break;
		treev_arrange_recursive( node, subtree_r0, reshape_tree );
		arc_width = DIR_NODE_DESC(node)->deployment * MAX(TREEV_GEOM_PARAMS(node)->platform.arc_width, TREEV_GEOM_PARAMS(node)->platform.subtree_arc_width);
		TREEV_GEOM_PARAMS(node)->platform.theta = arc_width; /* temporary value */
		subtree_arc_width += arc_width;
		node = node->next;
	}
	TREEV_GEOM_PARAMS(dnode)->platform.subtree_arc_width = subtree_arc_width;

	/* Spread the subdirectories, sweeping counterclockwise */
	theta = -0.5 * subtree_arc_width;
	node = dnode->children;
	while (node != NULL) {
                if (!NODE_IS_DIR(node))
			break;
		arc_width = TREEV_GEOM_PARAMS(node)->platform.theta;
		TREEV_GEOM_PARAMS(node)->platform.theta = theta + 0.5 * arc_width;
		theta += arc_width;
		node = node->next;
	}

	/* Clear the "need rearrange" flag */
	NODE_DESC(dnode)->flags &= ~TREEV_NEED_REARRANGE;
}


/* Top-level call to arrange the branches of the currently expanded tree,
 * as needed when directories collapse/expand (initial_arrange == FALSE),
 * or when tree is initially created (initial_arrange == TRUE) */
static void
treev_arrange( boolean initial_arrange )
{
	boolean resized = FALSE;

	treev_arrange_recursive( globals.fstree, treev_core_radius, initial_arrange );

	/* Check that the tree's total arc width is within bounds */
	for (;;) {
		if (TREEV_GEOM_PARAMS(globals.fstree)->platform.subtree_arc_width > TREEV_MAX_ARC_WIDTH) {
			/* Grow core radius */
			treev_core_radius *= TREEV_CORE_GROW_FACTOR;
			treev_arrange_recursive( globals.fstree, treev_core_radius, TRUE );
			resized = TRUE;
		}
		else if ((TREEV_GEOM_PARAMS(globals.fstree)->platform.subtree_arc_width < TREEV_MIN_ARC_WIDTH) && (TREEV_GEOM_PARAMS(globals.fstree)->platform.depth > TREEV_MIN_CORE_RADIUS)) {
			/* Shrink core radius */
			treev_core_radius = MAX(TREEV_MIN_CORE_RADIUS, treev_core_radius / TREEV_CORE_GROW_FACTOR);
			treev_arrange_recursive( globals.fstree, treev_core_radius, TRUE );
			resized = TRUE;
		}
		else
			break;
	}

	if (resized && camera_moving( )) {
		/* Camera's destination has moved, so it will need a
		 * flight path correction */
		camera_pan_break( );
		camera_look_at_full( globals.current_node, MORPH_INV_QUADRATIC, -1.0 );
	}
}


/* Helper function for treev_init( ) */
static void
treev_init_recursive( GNode *dnode )
{
	GNode *node;
	int64 size;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	if (NODE_IS_DIR(dnode)) {
		morph_break( &DIR_NODE_DESC(dnode)->deployment );
		if (dirtree_entry_expanded( dnode ))
			DIR_NODE_DESC(dnode)->deployment = 1.0;
		else
			DIR_NODE_DESC(dnode)->deployment = 0.0;
		geometry_queue_rebuild( dnode );
	}

	NODE_DESC(dnode)->flags = 0;

	/* Assign heights to leaf nodes */
	node = dnode->children;
	while (node != NULL) {
		size = MAX(64, NODE_DESC(node)->size);
		if (NODE_IS_DIR(node)) {
			size += DIR_NODE_DESC(node)->subtree.size;
			TREEV_GEOM_PARAMS(node)->platform.height = TREEV_PLATFORM_HEIGHT;
			treev_init_recursive( node );
		}
		TREEV_GEOM_PARAMS(node)->leaf.height = sqrt( (double)size ) * TREEV_LEAF_HEIGHT_MULTIPLIER;
		node = node->next;
	}
}


/* Top-level call to initialize TreeV mode */
static void
treev_init( void )
{
	TreeVGeomParams *gparams;
	int num_points;

	/* Allocate point buffers */
	num_points = (int)ceil( 360.0 / TREEV_CURVE_GRANULARITY ) + 1;
	if (inner_edge_buf == NULL)
		inner_edge_buf = NEW_ARRAY(XYvec, num_points);
	if (outer_edge_buf == NULL)
		outer_edge_buf = NEW_ARRAY(XYvec, num_points);

	treev_core_radius = TREEV_MIN_CORE_RADIUS;

	gparams = TREEV_GEOM_PARAMS(globals.fstree);
	gparams->platform.theta = 90.0;
	gparams->platform.depth = 0.0;
	gparams->platform.arc_width = TREEV_MAX_ARC_WIDTH;
	gparams->platform.height = 0.0;

	gparams = TREEV_GEOM_PARAMS(root_dnode);
	gparams->leaf.theta = 0.0;
	gparams->leaf.distance = (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	gparams->platform.theta = 0.0;

	treev_init_recursive( globals.fstree );
	treev_arrange( TRUE );

	/* Initial cursor state */
	treev_get_corners( root_dnode, &treev_cursor_prev_c0, &treev_cursor_prev_c1 );
	treev_cursor_prev_c0.r *= 0.875;
	treev_cursor_prev_c0.theta -= TREEV_GEOM_PARAMS(root_dnode)->platform.arc_width;
        treev_cursor_prev_c0.z = 0.0;
	treev_cursor_prev_c1.r *= 1.125;
	treev_cursor_prev_c1.theta += TREEV_GEOM_PARAMS(root_dnode)->platform.arc_width;
	treev_cursor_prev_c1.z = TREEV_GEOM_PARAMS(root_dnode)->platform.height;
}


/* Hook function for camera pan completion */
static void
treev_camera_pan_finished( void )
{
	/* Save cursor position */
	treev_get_corners( globals.current_node, &treev_cursor_prev_c0, &treev_cursor_prev_c1 );
}


/* Called by a directory as it collapses or expands (from the morph's step
 * callback; see colexp.c). This sets all the necessary flags to allow the
 * directory to move side to side without problems */
static void
treev_queue_rearrange( GNode *dnode )
{
	GNode *up_node;

	g_assert( NODE_IS_DIR(dnode) );

	up_node = dnode;
	while (up_node != NULL) {
		NODE_DESC(up_node)->flags |= TREEV_NEED_REARRANGE;

		/* Branch geometry has to be rebuilt (display list B) */
		DIR_NODE_DESC(up_node)->b_dlist_stale = TRUE;

		up_node = up_node->parent;
	}

	queue_uncached_draw( );
}


/* Draws a directory platform, with inner radius of r0 */
static void
treev_gldraw_platform( GNode *dnode, double r0 )
{
	XYvec p0, p1;
	XYvec delta;
	double r1, seg_arc_width;
	double theta, sin_theta, cos_theta;
	double z1;
	int s, seg_count;

	g_assert( NODE_IS_DIR(dnode) );

	r1 = r0 + TREEV_GEOM_PARAMS(dnode)->platform.depth;
	seg_count = (int)ceil( TREEV_GEOM_PARAMS(dnode)->platform.arc_width / TREEV_CURVE_GRANULARITY );
	seg_arc_width = TREEV_GEOM_PARAMS(dnode)->platform.arc_width / (double)seg_count;

	/* Calculate and cache inner/outer edge vertices */
	theta = -0.5 * TREEV_GEOM_PARAMS(dnode)->platform.arc_width;
	for (s = 0; s <= seg_count; s++) {
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = r0 * cos_theta;
		p0.y = r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = r1 * cos_theta;
		p1.y = r1 * sin_theta;
		if (s == 0) {
			/* Leading edge offset */
			delta.x = - sin_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			delta.y = cos_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			p0.x += delta.x;
			p0.y += delta.y;
			p1.x += delta.x;
			p1.y += delta.y;
		}
		else if (s == seg_count) {
			/* Trailing edge offset */
			delta.x = sin_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			delta.y = - cos_theta * (0.5 * TREEV_PLATFORM_SPACING_WIDTH);
			p0.x += delta.x;
			p0.y += delta.y;
			p1.x += delta.x;
			p1.y += delta.y;
		}

		/* cache */
		inner_edge_buf[s].x = p0.x;
		inner_edge_buf[s].y = p0.y;
		outer_edge_buf[s].x = p1.x;
		outer_edge_buf[s].y = p1.y;

		theta += seg_arc_width;
	}

	/* Height of top face */
        z1 = TREEV_GEOM_PARAMS(dnode)->platform.height;

	/* Everything here is done with quads */
	glBegin( GL_QUADS );

	/* Draw inner edge */
	for (s = 0; s < seg_count; s++) {
		/* Going up */
		p0.x = inner_edge_buf[s].x;
		p0.y = inner_edge_buf[s].y;
		glNormal3d( - p0.x / r0, - p0.y / r0, 0.0 );
		if (s > 0) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p0.x, p0.y, 0.0 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p0.x, p0.y, 0.0 );
		glVertex3d( p0.x, p0.y, z1 );

		/* Going down */
		p0.x = inner_edge_buf[s + 1].x;
		p0.y = inner_edge_buf[s + 1].y;
		glNormal3d( - p0.x / r0, - p0.y / r0, 0.0 );
		if ((s + 1) < seg_count) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p0.x, p0.y, z1 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p0.x, p0.y, z1 );
		glVertex3d( p0.x, p0.y, 0.0 );
	}

	/* Draw outer edge */
	for (s = seg_count; s > 0; s--) {
		/* Going up */
		p1.x = outer_edge_buf[s].x;
		p1.y = outer_edge_buf[s].y;
		glNormal3d( - p1.x / r1, - p1.y / r1, 0.0 );
		if (s < seg_count) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p1.x, p1.y, 0.0 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p1.x, p1.y, 0.0 );
		glVertex3d( p1.x, p1.y, z1 );

		/* Going down */
		p1.x = outer_edge_buf[s - 1].x;
		p1.y = outer_edge_buf[s - 1].y;
		glNormal3d( - p1.x / r1, - p1.y / r1, 0.0 );
		if ((s - 1) > 0) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p1.x, p1.y, z1 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p1.x, p1.y, z1 );
		glVertex3d( p1.x, p1.y, 0.0 );
	}

	/* Draw leading edge face */
	p0.x = inner_edge_buf[0].x;
	p0.y = inner_edge_buf[0].y;
	p1.x = outer_edge_buf[0].x;
	p1.y = outer_edge_buf[0].y;
	glNormal3d( p0.y / r0, - p0.x / r0, 0.0 );
	glVertex3d( p0.x, p0.y, 0.0 );
	glVertex3d( p1.x, p1.y, 0.0 );
	glVertex3d( p1.x, p1.y, z1 );
	glVertex3d( p0.x, p0.y, z1 );

	/* Draw trailing edge face */
	p0.x = inner_edge_buf[seg_count].x;
	p0.y = inner_edge_buf[seg_count].y;
	p1.x = outer_edge_buf[seg_count].x;
	p1.y = outer_edge_buf[seg_count].y;
	glNormal3d( - p0.y / r0, p0.x / r0, 0.0 );
	glVertex3d( p0.x, p0.y, z1 );
	glVertex3d( p1.x, p1.y, z1 );
	glVertex3d( p1.x, p1.y, 0.0 );
	glVertex3d( p0.x, p0.y, 0.0 );

	glEnd( );
	/* Top face has ID of 1 */
	glPushName( 1 );
	glBegin( GL_QUADS );

	/* Draw top face */
	glNormal3d( 0.0, 0.0, 1.0 );
	for (s = 0; s < seg_count; s++) {
		/* Going out */
		p0.x = inner_edge_buf[s].x;
		p0.y = inner_edge_buf[s].y;
		p1.x = outer_edge_buf[s].x;
		p1.y = outer_edge_buf[s].y;
		if (s > 0) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p0.x, p0.y, z1 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p0.x, p0.y, z1 );
		glVertex3d( p1.x, p1.y, z1 );

		/* Going in */
		p0.x = inner_edge_buf[s + 1].x;
		p0.y = inner_edge_buf[s + 1].y;
		p1.x = outer_edge_buf[s + 1].x;
		p1.y = outer_edge_buf[s + 1].y;
		if ((s + 1) < seg_count) {
			glEdgeFlag( GL_FALSE );
			glVertex3d( p1.x, p1.y, z1 );
			glEdgeFlag( GL_TRUE );
		}
		else
			glVertex3d( p1.x, p1.y, z1 );
		glVertex3d( p0.x, p0.y, z1 );
	}

	glEnd( );
	glPopName( );
}


/* Draws a leaf node. r0 is inner radius of parent; full_node flag
 * specifies whether the full leaf body should be drawn (TRUE) or merely
 * its "footprint" (FALSE). Note: Transformation matrix should be the same
 * one used to draw the underlying parent directory */
static void
treev_gldraw_leaf( GNode *node, double r0, boolean full_node )
{
	static const int x_verts[] = { 0, 2, 1, 3 };
	XYvec corners[4], p;
	double z0, z1;
	double edge, height;
	double sin_theta, cos_theta;
	int i;

	if (full_node) {
		edge = TREEV_LEAF_NODE_EDGE;
		height = TREEV_GEOM_PARAMS(node)->leaf.height;
		if (NODE_IS_DIR(node))
			height *= (1.0 - DIR_NODE_DESC(node)->deployment);
	}
	else {
		edge = (0.875 * TREEV_LEAF_NODE_EDGE);
		height = (TREEV_LEAF_NODE_EDGE / 64.0);
	}

	/* Set up corners, centered around (r0+distance,0,0) */

	/* Left/front */
	corners[0].x = r0 + TREEV_GEOM_PARAMS(node)->leaf.distance - 0.5 * edge;
	corners[0].y = -0.5 * edge;

	/* Right/front */
	corners[1].x = corners[0].x + edge;
	corners[1].y = corners[0].y;

	/* Right/rear */
	corners[2].x = corners[1].x;
	corners[2].y = corners[0].y + edge;

	/* Left/rear */
	corners[3].x = corners[0].x;
	corners[3].y = corners[2].y;

	/* Bottom and top */
	z0 = TREEV_GEOM_PARAMS(node->parent)->platform.height;
	z1 = z0 + height;

	sin_theta = sin( RAD(TREEV_GEOM_PARAMS(node)->leaf.theta) );
	cos_theta = cos( RAD(TREEV_GEOM_PARAMS(node)->leaf.theta) );

	/* Rotate corners into position (no glRotated( )-- leaf nodes are
	 * not important enough to mess with the transformation matrix) */
	for (i = 0; i < 4; i++) {
		p.x = corners[i].x;
		p.y = corners[i].y;
		corners[i].x = p.x * cos_theta - p.y * sin_theta;
		corners[i].y = p.x * sin_theta + p.y * cos_theta;
	}

	/* Draw top face */
	glNormal3d( 0.0, 0.0, 1.0 );
	glBegin( GL_QUADS );
	for (i = 0; i < 4; i++)
		glVertex3d( corners[i].x, corners[i].y, z1 );
	glEnd( );

	if (!full_node) {
		/* Draw an "X" and we're done */
		glBegin( GL_LINES );
		for (i = 0; i < 4; i++)
			glVertex3d( corners[x_verts[i]].x, corners[x_verts[i]].y, z1 );
		glEnd( );
		return;
	}

	/* Draw side faces */
	glBegin( GL_QUAD_STRIP );
	for (i = 0; i < 4; i++) {
		switch (i) {
			case 0:
			glNormal3d( sin_theta, - cos_theta, 0.0 );
			break;

			case 1:
			glNormal3d( cos_theta, sin_theta, 0.0 );
			break;

			case 2:
			glNormal3d( - sin_theta, cos_theta, 0.0 );
			break;

			case 3:
			glNormal3d( - cos_theta, - sin_theta, 0.0 );
			break;

			SWITCH_FAIL
		}
		glVertex3d( corners[i].x, corners[i].y, z1 );
		glVertex3d( corners[i].x, corners[i].y, z0 );
	}
	/* Close the strip */
	glVertex3d( corners[0].x, corners[0].y, z1 );
	glVertex3d( corners[0].x, corners[0].y, z0 );
	glEnd( );
}


/* Draws a "folder" shape on top of the given directory leaf node */
static void
treev_gldraw_folder( GNode *dnode, double r0 )
{
#define X1 (-0.4375 * TREEV_LEAF_NODE_EDGE)
#define X2 (0.375 * TREEV_LEAF_NODE_EDGE)
#define X3 (0.4375 * TREEV_LEAF_NODE_EDGE)
#define Y1 (-0.4375 * TREEV_LEAF_NODE_EDGE)
#define Y2 (Y1 + (2.0 - MAGIC_NUMBER) * TREEV_LEAF_NODE_EDGE)
#define Y3 (Y2 + 0.0625 * TREEV_LEAF_NODE_EDGE)
#define Y4 (Y5 - 0.0625 * TREEV_LEAF_NODE_EDGE)
#define Y5 (0.4375 * TREEV_LEAF_NODE_EDGE)
	static const XYvec folder_points[] = {
		{ X1, Y1 },
		{ X2, Y1 },
		{ X2, Y2 },
		{ X3, Y3 },
		{ X3, Y4 },
		{ X2, Y5 },
		{ X1, Y5 }
	};
#undef X1
#undef X2
#undef X3
#undef Y1
#undef Y2
#undef Y3
#undef Y4
#undef Y5
	XYZvec p_rot;
	XYvec p;
	double folder_r;
	double sin_theta, cos_theta;
	int i;

	g_assert( NODE_IS_DIR(dnode) );

	folder_r = r0 + TREEV_GEOM_PARAMS(dnode)->leaf.distance;
	sin_theta = sin( RAD(TREEV_GEOM_PARAMS(dnode)->leaf.theta) );
	cos_theta = cos( RAD(TREEV_GEOM_PARAMS(dnode)->leaf.theta) );
	p_rot.z = (1.0 - DIR_NODE_DESC(dnode)->deployment) * TREEV_GEOM_PARAMS(dnode)->leaf.height + TREEV_GEOM_PARAMS(dnode->parent)->platform.height;

	/* Translate, rotate, and draw folder geometry */
        node_glcolor( dnode );
	glBegin( GL_LINE_STRIP );
	for (i = 0; i <= 7; i++) {
		p.x = folder_r + folder_points[i % 7].x;
		p.y = folder_points[i % 7].y;
		p_rot.x = p.x * cos_theta - p.y * sin_theta;
		p_rot.y = p.x * sin_theta + p.y * cos_theta;

		glVertex3d( p_rot.x, p_rot.y, p_rot.z );
	}
	glEnd( );
}


/* Draws the loop around the TreeV center, with the given radius */
static void
treev_gldraw_loop( double loop_r )
{
	static const int seg_count = (int)(360.0 / TREEV_CURVE_GRANULARITY + 0.5);
	XYvec p0, p1;
	double loop_r0, loop_r1;
	double theta, sin_theta, cos_theta;
	int s;

	/* Inner/outer loop radii */
	loop_r0 = loop_r - (0.5 * TREEV_BRANCH_WIDTH);
	loop_r1 = loop_r + (0.5 * TREEV_BRANCH_WIDTH);

	/* Draw loop */
	glBegin( GL_QUAD_STRIP );
	for (s = 0; s <= seg_count; s++) {
		theta = 360.0 * (double)s / (double)seg_count;
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = loop_r0 * cos_theta;
		p0.y = loop_r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = loop_r1 * cos_theta;
		p1.y = loop_r1 * sin_theta;

		glVertex2d( p0.x, p0.y );
		glVertex2d( p1.x, p1.y );
	}
	glEnd( );
}


/* Draws part of the branch connecting to the inner edge of a platform.
 * r0 is the platform's inner radius */
static void
treev_gldraw_inbranch( double r0 )
{
	XYvec c0, c1;

	/* Left/front */
	c0.x = r0 - (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	c0.y = (-0.5 * TREEV_BRANCH_WIDTH);

	/* Right/rear */
	c1.x = r0;
	c1.y = (0.5 * TREEV_BRANCH_WIDTH);

	glBegin( GL_QUADS );
	glVertex2d( c0.x, c0.y );
	glVertex2d( c1.x, c0.y );
	glVertex2d( c1.x, c1.y );
	glVertex2d( c0.x, c1.y );
	glEnd( );
}


/* Draws part of the branch present on the outer edge of platforms with
 * expanded subdirectories. r1 is the outer radius of the parent directory,
 * and theta0/theta1 are the start/end angles of the arc portion */
static void
treev_gldraw_outbranch( double r1, double theta0, double theta1 )
{
	XYvec p0, p1;
	double arc_r, arc_r0, arc_r1;
	double arc_width, seg_arc_width;
	double supp_arc_width;
	double theta, sin_theta, cos_theta;
	int s, seg_count;

	g_assert( theta1 >= theta0 );

	/* Radii of branch arc (middle, inner, outer) */
	arc_r = r1 + (0.5 * TREEV_PLATFORM_SPACING_DEPTH);
	arc_r0 = arc_r - (0.5 * TREEV_BRANCH_WIDTH);
	arc_r1 = arc_r + (0.5 * TREEV_BRANCH_WIDTH);

	/* Left/front of stem */
	p0.x = r1;
	p0.y = (-0.5 * TREEV_BRANCH_WIDTH);

	/* Right/rear of stem */
	p1.x = arc_r;
	p1.y = (0.5 * TREEV_BRANCH_WIDTH);

	/* Draw branch stem */
	glBegin( GL_QUADS );
	glVertex2d( p0.x, p0.y );
	glVertex2d( p1.x, p0.y );
	glVertex2d( p1.x, p1.y );
	glVertex2d( p0.x, p1.y );
	glEnd( );

	/* Shortcut: If arc is zero-length, don't bother drawing it */
	arc_width = theta1 - theta0;
	if (arc_width < EPSILON)
		return;

	/* Supplemental arc width, to yield fully square branch corners
	 * (where directories connect to the ends of the arc) */
	supp_arc_width = (180.0 * TREEV_BRANCH_WIDTH / PI) / arc_r0;

	seg_count = (int)ceil( (arc_width + supp_arc_width) / TREEV_CURVE_GRANULARITY );
	seg_arc_width = (arc_width + supp_arc_width) / (double)seg_count;

	/* Draw branch arc */
	glBegin( GL_QUAD_STRIP );
	theta = theta0 - 0.5 * supp_arc_width;
	for (s = 0; s <= seg_count; s++) {
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );
		/* p0: point on inner edge */
		p0.x = arc_r0 * cos_theta;
		p0.y = arc_r0 * sin_theta;
		/* p1: point on outer edge */
		p1.x = arc_r1 * cos_theta;
		p1.y = arc_r1 * sin_theta;

		glVertex2d( p0.x, p0.y );
		glVertex2d( p1.x, p1.y );

		theta += seg_arc_width;
	}
	glEnd( );
}


/* Arranges/draws leaf nodes on a directory */
static void
treev_build_dir( GNode *dnode, double r0 )
{
#define edge05 (0.5 * TREEV_LEAF_NODE_EDGE)
#define edge15 (1.5 * TREEV_LEAF_NODE_EDGE)
	GNode *node;
	RTvec pos;
	double arc_len, inter_arc_width;
	int n, row_node_count, remaining_node_count;

	g_assert( NODE_IS_DIR(dnode) );

	/* Build rows of leaf nodes, going from the inner edge outward
	 * (this will require laying down nodes in reverse order) */
	remaining_node_count = g_list_length( (GList *)dnode->children );
	pos.r = r0 + TREEV_LEAF_NODE_EDGE;
	node = (GNode *)g_list_last( (GList *)dnode->children );
	while (node != NULL) {
		/* Calculate (available) arc length of row */
		arc_len = (PI / 180.0) * pos.r * TREEV_GEOM_PARAMS(dnode)->platform.arc_width - TREEV_PLATFORM_SPACING_WIDTH;
		/* Number of nodes this row can accomodate */
		row_node_count = (int)floor( (arc_len - edge05) / edge15 );
		/* Arc width between adjacent leaf nodes */
		inter_arc_width = (180.0 * edge15 / PI) / pos.r;

		/* Lay out nodes in this row, sweeping clockwise */
		pos.theta = 0.5 * inter_arc_width * (double)(MIN(row_node_count, remaining_node_count) - 1);
		for (n = 0; (n < row_node_count) && (node != NULL); n++) {
			TREEV_GEOM_PARAMS(node)->leaf.theta = pos.theta;
			TREEV_GEOM_PARAMS(node)->leaf.distance = pos.r - r0;
			glLoadName( NODE_DESC(node)->id );
			node_glcolor( node );
			treev_gldraw_leaf( node, r0, !NODE_IS_DIR(node) );
			pos.theta -= inter_arc_width;
			node = node->prev;
		}

		remaining_node_count -= row_node_count;
		pos.r += edge15;
	}

	/* Official directory depth */
	pos.r -= edge05;
	TREEV_GEOM_PARAMS(dnode)->platform.depth = pos.r - r0;

	/* Draw underlying directory */
	glLoadName( NODE_DESC(dnode)->id );
	node_glcolor( dnode );
	treev_gldraw_platform( dnode, r0 );

#undef edge05
#undef edge15
}


/* Draws a node name label. is_leaf indicates whether the given node should
 * be labeled as a leaf, or as a directory platform (if applicable) */
static void
treev_apply_label( GNode *node, double r0, boolean is_leaf )
{
	RTZvec label_pos;
	XYvec leaf_label_dims;
	RTvec platform_label_dims;
	double height;

	if (is_leaf) {
		/* Apply label to top face of leaf node */
		height = TREEV_GEOM_PARAMS(node)->leaf.height;
		if (NODE_IS_DIR(node)) {
			height *= (1.0 - DIR_NODE_DESC(node)->deployment);
			leaf_label_dims.x = (0.8125 * TREEV_LEAF_NODE_EDGE);
		}
		else
			leaf_label_dims.x = (0.875 * TREEV_LEAF_NODE_EDGE);
		leaf_label_dims.y = ((2.0 - MAGIC_NUMBER) * TREEV_LEAF_NODE_EDGE);
		label_pos.r = r0 + TREEV_GEOM_PARAMS(node)->leaf.distance;
		label_pos.theta = TREEV_GEOM_PARAMS(node)->leaf.theta;
		label_pos.z = height + TREEV_GEOM_PARAMS(node->parent)->platform.height;
		text_draw_straight_rotated( NODE_DESC(node)->name, &label_pos, &leaf_label_dims );
	}
	else {
		/* Label directory platform, inside its inner edge */
		label_pos.r = r0 - (0.0625 * TREEV_PLATFORM_SPACING_DEPTH);
		label_pos.theta = 0.0;
		label_pos.z = 0.0;
		platform_label_dims.r = ((2.0 - MAGIC_NUMBER) * TREEV_PLATFORM_SPACING_DEPTH);
		platform_label_dims.theta = TREEV_GEOM_PARAMS(node)->platform.arc_width - (180.0 * TREEV_PLATFORM_SPACING_WIDTH / PI) / label_pos.r;
		text_draw_curved( NODE_DESC(node)->name, &label_pos, &platform_label_dims );
	}
}


/* TreeV mode "full draw" */
static boolean
treev_draw_recursive( GNode *dnode, double prev_r0, double r0, int action )
{
	DirNodeDesc *dir_ndesc;
	TreeVGeomParams *dir_gparams;
	GNode *node;
	GNode *first_node = NULL, *last_node = NULL;
	RTvec leaf;
	double subtree_r0;
	double theta0, theta1;
	boolean dir_collapsed;
	boolean dir_expanded;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );
	dir_ndesc = DIR_NODE_DESC(dnode);
	dir_gparams = TREEV_GEOM_PARAMS(dnode);

	glPushMatrix( );

	dir_collapsed = DIR_COLLAPSED(dnode);
        dir_expanded = DIR_EXPANDED(dnode);

	if (!dir_collapsed) {
		if (!dir_expanded) {
			/* Directory is partially deployed, so
			 * draw/label the shrinking/growing leaf */
			if (action >= TREEV_DRAW_GEOMETRY) {
				node_glcolor( dnode );
				treev_gldraw_leaf( dnode, prev_r0, TRUE );
				treev_gldraw_folder( dnode, prev_r0 );
			}
			else if (action == TREEV_DRAW_LABELS) {
				glColor3fv( (float *)&treev_leaf_label_color );
				treev_apply_label( dnode, prev_r0, TRUE );
			}

			/* Platform should shrink to / grow from
			 * corresponding leaf position */
			glEnable( GL_NORMALIZE );
			leaf.r = prev_r0 + dir_gparams->leaf.distance;
			leaf.theta = dir_gparams->leaf.theta;
			glRotated( leaf.theta, 0.0, 0.0, 1.0 );
			glTranslated( leaf.r, 0.0, 0.0 );
			glScaled( dir_ndesc->deployment, dir_ndesc->deployment, dir_ndesc->deployment );
			glTranslated( - leaf.r, 0.0, 0.0 );
			glRotated( - leaf.theta, 0.0, 0.0, 1.0 );
		}

		glRotated( dir_gparams->platform.theta, 0.0, 0.0, 1.0 );
	}

	if (action >= TREEV_DRAW_GEOMETRY) {
		/* Draw directory, in either leaf or platform form
		 * (display list A) */
		if (dir_ndesc->a_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->a_dlist == NULL_DLIST)
				dir_ndesc->a_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->a_dlist, GL_COMPILE_AND_EXECUTE );
			if (dir_collapsed) {
				/* Leaf form */
				glLoadName( NODE_DESC(dnode)->id );
				node_glcolor( dnode );
				treev_gldraw_leaf( dnode, prev_r0, TRUE );
				treev_gldraw_folder( dnode, prev_r0 );
			}
			else if (NODE_IS_DIR(dnode)) {
				/* Platform form (with leaf children) */
				treev_build_dir( dnode, r0 );
			}
			glEndList( );
			dir_ndesc->a_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->a_dlist );
	}

	if (!dir_collapsed) {
		/* Recurse into subdirectories */
		subtree_r0 = r0 + dir_gparams->platform.depth + TREEV_PLATFORM_SPACING_DEPTH;
		node = dnode->children;
		while (node != NULL) {
			if (!NODE_IS_DIR(node))
				break;
			if (treev_draw_recursive( node, r0, subtree_r0, action )) {
				/* This subdirectory is expanded.
				 * Save first/last node information for
				 * drawing interconnecting branches */
				if (first_node == NULL)
					first_node = node;
				last_node = node;
			}
			node = node->next;
		}
	}

	if (dir_expanded && (action == TREEV_DRAW_GEOMETRY_WITH_BRANCHES)) {
		/* Draw interconnecting branches (display list B) */
		if (dir_ndesc->b_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->b_dlist == NULL_DLIST)
				dir_ndesc->b_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->b_dlist, GL_COMPILE_AND_EXECUTE );
			glLoadName( NODE_DESC(dnode)->id );
			glColor3fv( (float *)&branch_color );
			glNormal3d( 0.0, 0.0, 1.0 );
			if (NODE_IS_METANODE(dnode)) {
				treev_gldraw_loop( r0 );
				treev_gldraw_outbranch( r0, 0.0, 0.0 );
			}
			else {
				treev_gldraw_inbranch( r0 );
				if (first_node != NULL) {
					theta0 = MIN(0.0, TREEV_GEOM_PARAMS(first_node)->platform.theta);
					theta1 = MAX(0.0, TREEV_GEOM_PARAMS(last_node)->platform.theta);
					treev_gldraw_outbranch( r0 + dir_gparams->platform.depth, theta0, theta1 );
				}
			}
			glEndList( );
			dir_ndesc->b_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->b_dlist );
	}

	if (action == TREEV_DRAW_LABELS) {
		/* Draw name label(s) (display list C) */
		if (dir_ndesc->c_dlist_stale) {
			/* Rebuild */
			if (dir_ndesc->c_dlist == NULL_DLIST)
				dir_ndesc->c_dlist = glGenLists( 1 );
			glNewList( dir_ndesc->c_dlist, GL_COMPILE_AND_EXECUTE );
			if (dir_collapsed) {
				/* Label directory leaf */
				glColor3fv( (float *)&treev_leaf_label_color );
				treev_apply_label( dnode, prev_r0, TRUE );
			}
			else if (NODE_IS_DIR(dnode)) {
				/* Label directory platform */
				glColor3fv( (float *)&treev_platform_label_color );
				treev_apply_label( dnode, r0, FALSE );
				/* Label leaf nodes that aren't directories */
				glColor3fv( (float *)&treev_leaf_label_color );
				node = dnode->children;
				while (node != NULL) {
					if (!NODE_IS_DIR(node))
						treev_apply_label( node, r0, TRUE );
					node = node->next;
				}
			}
			glEndList( );
			dir_ndesc->c_dlist_stale = FALSE;
		}
		else
			glCallList( dir_ndesc->c_dlist );
	}

	/* Update geometry status */
	dir_ndesc->geom_expanded = !dir_collapsed;

	if (!dir_collapsed && !dir_expanded)
		glDisable( GL_NORMALIZE );

	glPopMatrix( );

	return dir_expanded;
}


/* Draws the node cursor, size/position specified by corners */
static void
treev_gldraw_cursor( RTZvec *c0, RTZvec *c1 )
{
	static const double bar_part = SQR(SQR(MAGIC_NUMBER - 1.0));
	RTZvec corner_dims;
	RTZvec p, delta;
	XYvec cp0, cp1;
	double theta;
	double sin_theta, cos_theta;
	int seg_count;
	int i, c, s;

	g_assert( c1->r > c0->r );
	g_assert( c1->theta > c0->theta );
	g_assert( c1->z > c0->z );

	corner_dims.r = bar_part * (c1->r - c0->r);
	corner_dims.theta = bar_part * (c1->theta - c0->theta);
	corner_dims.z = bar_part * (c1->z - c0->z);

	seg_count = (int)ceil( corner_dims.theta / TREEV_CURVE_GRANULARITY );

	cursor_pre( );
	for (i = 0; i <= 1; i++) {
		if (i == 0)
			cursor_hidden_part( );
		if (i == 1)
			cursor_visible_part( );

		for (c = 0; c < 8; c++) {
			if (c & 1) {
				p.r = c1->r;
				delta.r = - corner_dims.r;
			}
			else {
				p.r = c0->r;
				delta.r = corner_dims.r;
			}

			if (c & 2) {
				p.theta = c1->theta;
				delta.theta = - corner_dims.theta;
			}
			else {
				p.theta = c0->theta;
				delta.theta = corner_dims.theta;
			}

			if (c & 4) {
				p.z = c1->z;
				delta.z = - corner_dims.z;
			}
			else {
				p.z = c0->z;
				delta.z = corner_dims.z;
			}

			sin_theta = sin( RAD(p.theta) );
			cos_theta = cos( RAD(p.theta) );
			cp0.x = p.r * cos_theta;
			cp0.y = p.r * sin_theta;
			cp1.x = (p.r + delta.r) * cos_theta;
			cp1.y = (p.r + delta.r) * sin_theta;
			glBegin( GL_LINES );
			/* Radial axis */
			glVertex3d( cp0.x, cp0.y, p.z );
			glVertex3d( cp1.x, cp1.y, p.z );
			/* Vertical axis */
			glVertex3d( cp0.x, cp0.y, p.z );
			glVertex3d( cp0.x, cp0.y, p.z + delta.z );
			glEnd( );

			/* Tangent axis (curved part) */
			glBegin( GL_LINE_STRIP );
			for (s = 0; s <= seg_count; s++) {
				theta = p.theta + delta.theta * (double)s / (double)seg_count;
				cp0.x = p.r * cos( RAD(theta) );
				cp0.y = p.r * sin( RAD(theta) );
				glVertex3d( cp0.x, cp0.y, p.z );
			}
			glEnd( );
		}
	}
	cursor_post( );
}


/* Draws the node cursor in an intermediate position between its previous
 * steady-state position and the current node (pos=0 indicates the former,
 * pos=1 the latter) */
static void
treev_draw_cursor( double pos )
{
	RTZvec c0, c1;
	RTZvec cursor_c0, cursor_c1;

	treev_get_corners( globals.current_node, &c0, &c1 );

	/* Interpolate corners */
	cursor_c0.r = INTERPOLATE(pos, treev_cursor_prev_c0.r, c0.r);
	cursor_c0.theta = INTERPOLATE(pos, treev_cursor_prev_c0.theta, c0.theta);
	cursor_c0.z = INTERPOLATE(pos, treev_cursor_prev_c0.z, c0.z);
	cursor_c1.r = INTERPOLATE(pos, treev_cursor_prev_c1.r, c1.r);
	cursor_c1.theta = INTERPOLATE(pos, treev_cursor_prev_c1.theta, c1.theta);
	cursor_c1.z = INTERPOLATE(pos, treev_cursor_prev_c1.z, c1.z);

	treev_gldraw_cursor( &cursor_c0, &cursor_c1 );
}


/* Draws TreeV geometry */
static void
treev_draw( boolean high_detail )
{
	if ((fstree_low_draw_stage == 0) || (fstree_high_draw_stage == 0))
		treev_arrange( FALSE );

	/* Draw low-detail geometry */

	if (fstree_low_draw_stage == 1)
		glNewList( fstree_low_dlist, GL_COMPILE_AND_EXECUTE );

	if (fstree_low_draw_stage <= 1)
		treev_draw_recursive( globals.fstree, NIL, treev_core_radius, TREEV_DRAW_GEOMETRY_WITH_BRANCHES );
	else
		glCallList( fstree_low_dlist );

	if (fstree_low_draw_stage == 1)
		glEndList( );
	if (fstree_low_draw_stage <= 1)
		++fstree_low_draw_stage;

	if (high_detail) {
		/* Draw additional high-detail stuff */

		if (fstree_high_draw_stage == 1)
			glNewList( fstree_high_dlist, GL_COMPILE_AND_EXECUTE );

		if (fstree_high_draw_stage <= 1) {
			/* "Cel lines" */
			outline_pre( );
			treev_draw_recursive( globals.fstree, NIL, treev_core_radius, TREEV_DRAW_GEOMETRY );
			outline_post( );

			/* Node name labels */
			text_pre( );
			treev_draw_recursive( globals.fstree, NIL, treev_core_radius, TREEV_DRAW_LABELS );
			text_post( );
		}
		else
			glCallList( fstree_high_dlist );

		if (fstree_high_draw_stage == 1)
			glEndList( );
		if (fstree_high_draw_stage <= 1)
			++fstree_high_draw_stage;

		/* Node cursor */
		treev_draw_cursor( CURSOR_POS(camera->pan_part) );
	}
}


/**** COMMON ROUTINES *****************************************/


/* Call before drawing geometry "cel lines" */
static void
outline_pre( void )
{
	glDisable( GL_CULL_FACE );
	glDisable( GL_LIGHT0 );
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
}


/* Call after drawing "cel lines" */
static void
outline_post( void )
{
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glEnable( GL_LIGHT0 );
	glEnable( GL_CULL_FACE );
}


/* Call before drawing the cursor */
static void
cursor_pre( void )
{
	glDisable( GL_LIGHTING );
}


/* Call to draw the "hidden" part of the cursor */
static void
cursor_hidden_part( void )
{
	/* Hidden part is drawn with a thin dashed line */
	glDepthFunc( GL_GREATER );
	glEnable( GL_LINE_STIPPLE );
	glLineStipple( 3, 0x3333 );
	glLineWidth( 3.0 );
	glColor3f( 0.75, 0.75, 0.75 );
}


/* Call to draw the visible part of the cursor */
static void
cursor_visible_part( void )
{
	/* Visible part is drawn with a thick solid line */
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_LINE_STIPPLE );
	glLineWidth( 5.0 );
	glColor3f( 1.0, 1.0, 1.0 );
}


/* Call after drawing the cursor */
static void
cursor_post( void )
{
	glLineWidth( 1.0 );
	glEnable( GL_LIGHTING );
}


/* Zeroes the drawing stages for both low- and high-detail geometry, so
 * that a full recursive draw is performed in the next frame without
 * the use of display lists (i.e. caches). This is necessary whenever
 * any geometry needs to be (re)built */
static void
queue_uncached_draw( void )
{
	fstree_low_draw_stage = 0;
	fstree_high_draw_stage = 0;
}


/* Flags a directory's geometry for rebuilding */
void
geometry_queue_rebuild( GNode *dnode )
{
	DIR_NODE_DESC(dnode)->a_dlist_stale = TRUE;
	DIR_NODE_DESC(dnode)->b_dlist_stale = TRUE;
	DIR_NODE_DESC(dnode)->c_dlist_stale = TRUE;

	queue_uncached_draw( );
}


/* Sets up filesystem tree geometry for the specified mode */
void
geometry_init( FsvMode mode )
{
	/* Allocate filesystem display lists (first time only) */
	if (fstree_low_dlist == NULL_DLIST)
		fstree_low_dlist = glGenLists( 1 );
	if (fstree_high_dlist == NULL_DLIST)
		fstree_high_dlist = glGenLists( 1 );

	DIR_NODE_DESC(globals.fstree)->deployment = 1.0;
	geometry_queue_rebuild( globals.fstree );

	switch (mode) {
		case FSV_DISCV:
		discv_init( );
		break;

		case FSV_MAPV:
		mapv_init( );
		break;

		case FSV_TREEV:
		treev_init( );
		break;

		SWITCH_FAIL
	}

	color_assign_recursive( globals.fstree );
}


/* Draws "fsv" in 3D */
void
geometry_gldraw_fsv( void )
{
	XYvec p, n;
	const float *vertices = NULL;
	const int *triangles = NULL, *edges = NULL;
	int c, v, e, i;

	glEnable( GL_NORMALIZE );
	for (c = 0; c < 3; c++) {
		glColor3fv( (float *)&fsv_colors[c] );
		vertices = fsv_vertices[c];
		triangles = fsv_triangles[c];
		edges = fsv_edges[c];

		/* Side faces */
		glBegin( GL_QUAD_STRIP );
		for (e = 0; edges[e] >= 0; e++) {
			i = edges[e];
			p.x = vertices[2 * i];
			p.y = vertices[2 * i + 1];
			i = edges[e + 1];
			if (i >= 0) {
				n.x = vertices[2 * i + 1] - p.y;
				n.y = p.x - vertices[2 * i];
				glNormal3d( n.x, n.y, 0.0 );
			}
			glVertex3d( p.x, p.y, 30.0 );
			glVertex3d( p.x, p.y, -30.0 );

		}
		glEnd( );

		/* Front faces */
		glNormal3d( 0.0, 0.0, 1.0 );
		glBegin( GL_TRIANGLES );
		for (v = 0; triangles[v] >= 0; v++) {
                        i = triangles[v];
			p.x = vertices[2 * i];
			p.y = vertices[2 * i + 1];
			glVertex3d( p.x, p.y, 30.0 );
		}
		glEnd( );

		/* Back faces */
		glNormal3d( 0.0, 0.0, -1.0 );
		glBegin( GL_TRIANGLES );
		for (--v; v >= 0; v--) {
                        i = triangles[v];
			p.x = vertices[2 * i];
			p.y = vertices[2 * i + 1];
			glVertex3d( p.x, p.y, -30.0 );
		}
		glEnd( );
	}
	glDisable( GL_NORMALIZE );
}


/* Draws the splash screen */
static void
splash_draw( void )
{
	XYZvec text_pos;
	XYvec text_dims;
	double bottom_y;
	double k;

	/* Draw fsv title */

	/* Set up projection matrix */
	glMatrixMode( GL_PROJECTION );
	glPushMatrix( );
	glLoadIdentity( );
	k = 82.84 / ogl_aspect_ratio( );
	glFrustum( -70.82, 95.40, - k, k, 200.0, 400.0 );

	/* Set up modelview matrix */
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix( );
	glLoadIdentity( );
	glTranslated( 0.0, 0.0, -300.0 );
	glRotated( 10.5, 1.0, 0.0, 0.0 );
	glTranslated( 20.0, 20.0, -30.0 );

	geometry_gldraw_fsv( );

	/* Draw accompanying text */

	/* Set up projection matrix */
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity( );
	k = 0.5 / ogl_aspect_ratio( );
	glOrtho( 0.0, 1.0, - k, k, -1.0, 1.0 );
	bottom_y = - k;

	/* Set up modelview matrix */
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );

	text_pre( );

	/* Title */
	glColor3f( 1.0, 1.0, 1.0 );
	text_pos.x = 0.2059;
	text_pos.y = -0.1700;
	text_pos.z = 0.0;
	text_dims.x = 0.9;
	text_dims.y = 0.0625;
	text_draw_straight( "File", &text_pos, &text_dims );
	text_pos.x = 0.4449;
	text_draw_straight( "System", &text_pos, &text_dims );
	text_pos.x = 0.7456;
	text_draw_straight( "Visualizer", &text_pos, &text_dims );

	/* Version */
	glColor3f( 0.75, 0.75, 0.75 );
	text_pos.x = 0.5000;
	text_pos.y = (2.0 - MAGIC_NUMBER) * (0.2247 + bottom_y) - 0.2013;
	text_dims.y = 0.0386;
	text_draw_straight( "Version " VERSION, &text_pos, &text_dims );

	/* Copyright/author info */
	glColor3f( 0.5, 0.5, 0.5 );
	text_pos.y = bottom_y + 0.0117;
	text_dims.y = 0.0234;
	text_draw_straight( "Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>", &text_pos, &text_dims );

	text_post( );

	/* Restore previous matrices */
	glMatrixMode( GL_PROJECTION );
	glPopMatrix( );
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix( );
}


/* Top-level call to draw viewport content */
void
geometry_draw( boolean high_detail )
{
	/* Initialize name stack */
	glInitNames( );
	glPushName( 0 );

	if (about( ABOUT_CHECK )) {
		/* Currently giving About presentation */
		if (high_detail)
			about( ABOUT_DRAW );
		return;
	}

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		splash_draw( );
		break;

		case FSV_DISCV:
		discv_draw( high_detail );
		break;

		case FSV_MAPV:
		mapv_draw( high_detail );
		break;

		case FSV_TREEV:
		treev_draw( high_detail );
		break;

		SWITCH_FAIL
	}
}


/* This gets called upon completion of a camera pan */
void
geometry_camera_pan_finished( void )
{
	switch (globals.fsv_mode) {
		case FSV_DISCV:
		/* discv_camera_pan_finished( ); */
		break;

		case FSV_MAPV:
		mapv_camera_pan_finished( );
		break;

		case FSV_TREEV:
		treev_camera_pan_finished( );
		break;

		SWITCH_FAIL
	}
}


/* This is called when a directory is about to collapse or expand */
void
geometry_colexp_initiated( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	/* A newly expanding directory in TreeV mode will probably
	 * need (re)shaping (it may be appearing for the first time,
	 * or its inner radius may have changed) */
	if (DIR_COLLAPSED(dnode) && (globals.fsv_mode == FSV_TREEV))
		treev_reshape_platform( dnode, geometry_treev_platform_r0( dnode ) );
}


/* This is called as a directory collapses or expands (and also when it
 * finishes either operation) */
void
geometry_colexp_in_progress( GNode *dnode )
{
	g_assert( NODE_IS_DIR(dnode) );

	/* Check geometry status against deployment. If they don't concur
	 * properly, then directory geometry has to be rebuilt */
        if (DIR_NODE_DESC(dnode)->geom_expanded != (DIR_NODE_DESC(dnode)->deployment > EPSILON))
		geometry_queue_rebuild( dnode );
        else
		queue_uncached_draw( );

	if (globals.fsv_mode == FSV_TREEV) {
		/* Take care of shifting angles */
		treev_queue_rearrange( dnode );
	}
}


/* This tells if the specified node should be highlighted when the user
 * points at the specified face */
boolean
geometry_should_highlight( GNode *node, unsigned int face_id )
{
	if (!NODE_IS_DIR(node) || (face_id != 1))
		return TRUE;

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		return TRUE;

		case FSV_MAPV:
		return DIR_COLLAPSED(node);

		case FSV_TREEV:
		return geometry_treev_is_leaf( node );

		SWITCH_FAIL
	}

	return FALSE;
}


/* Draws a single node, in its absolute position */
static void
draw_node( GNode *node )
{
	glPushMatrix( );

	switch (globals.fsv_mode) {
		case FSV_DISCV:
		/* TODO: code to draw single discv node goes HERE */
		break;

		case FSV_MAPV:
		glTranslated( 0.0, 0.0, geometry_mapv_node_z0( node ) );
		mapv_gldraw_node( node );
		break;

		case FSV_TREEV:
		if (geometry_treev_is_leaf( node )) {
			glRotated( geometry_treev_platform_theta( node->parent ), 0.0, 0.0, 1.0 );
			treev_gldraw_leaf( node, geometry_treev_platform_r0( node->parent ), TRUE );
		}
		else {
			glRotated( geometry_treev_platform_theta( node ), 0.0, 0.0, 1.0 );
			treev_gldraw_platform( node, geometry_treev_platform_r0( node ) );
		}
		break;

		SWITCH_FAIL
	}

	glPopMatrix( );
}


/* Highlights a node. This isn't a draw function per se, as it manipulates
 * the front and back buffers directly to do its work. "strong" flag
 * indicates whether a noticeably heavier highlight should be drawn.
 * Passing NULL/FALSE clears the existing highlight;
 * passing NULL/TRUE resets internal state (ogl_draw( ) only, please) */
void
geometry_highlight_node( GNode *node, boolean strong )
{
	static boolean highlight_drawn = FALSE;
	static boolean highlight_strong = FALSE;
	static GNode *highlighted_node;
	static double prev_proj_matrix[16];
	static double prev_mview_matrix[16];
	static int prev_vp_x1, prev_vp_y1;
	static int prev_vp_x2, prev_vp_y2;
	GLfloat feedback_buf[1024];
	GLint viewport[4];
	int width, height;
	int vp_x, vp_y;
	int vp_x1 = INT_MAX, vp_y1 = INT_MAX;
	int vp_x2 = -1, vp_y2 = -1;
	int val_count;
	int i = 0, v;

	if ((node == NULL) && strong) {
		highlight_drawn = FALSE;
		return;
	}

	/* Disable active per-fragment operations
	 * (for a faster glCopyPixels( )) */
	glDisable( GL_DITHER );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_LIGHTING );

	if (highlight_drawn) {
		if ((node != highlighted_node) || (!strong && highlight_strong)) {
			/* Remove the previously drawn highlight
			 * (i.e. restore the previously saved portion
			 * of the screen) */
			glMatrixMode( GL_PROJECTION );
			glPushMatrix( );
			glLoadMatrixd( prev_proj_matrix );
			glMatrixMode( GL_MODELVIEW );
			glPushMatrix( );
			glLoadMatrixd( prev_mview_matrix );

			glReadBuffer( GL_BACK );
			glDrawBuffer( GL_FRONT );
			glCopyPixels( prev_vp_x1, prev_vp_y1, prev_vp_x2, prev_vp_y2, GL_COLOR );
			glDrawBuffer( GL_BACK );

			glMatrixMode( GL_PROJECTION );
			glPopMatrix( );
			glMatrixMode( GL_MODELVIEW );
			glPopMatrix( );
		}
		else if (strong == highlight_strong) {
			/* Desired highlight is already drawn */
			glEnable( GL_LIGHTING );
			glEnable( GL_DEPTH_TEST );
			glEnable( GL_DITHER );
			return;
		}
	}

	if ((node == NULL) && !strong) {
		/* Highlight has been cleared, we're done */
		highlight_drawn = FALSE;
		glEnable( GL_LIGHTING );
		glEnable( GL_DEPTH_TEST );
		glEnable( GL_DITHER );
		glFlush( );
		return;
	}

	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	if (!highlight_drawn || (node != highlighted_node)) {
		/* Use feedback to determine what portion of the screen
		 * will need to be saved */
		glFeedbackBuffer( 1024, GL_2D, feedback_buf );
		glRenderMode( GL_FEEDBACK );
		draw_node( node );
		val_count = glRenderMode( GL_RENDER );

		/* Parse values returned in feedback buffer */
		while (i < val_count) {
			i++; /* or: ptype = feedback_buf[i++]; */
			/* (ptype == GL_LINE_TOKEN or GL_LINE_RESET_TOKEN) */
			for (v = 0; v < 2; v++) {
				vp_x = (int)feedback_buf[i++];
				vp_y = (int)feedback_buf[i++];
				/* Find corners */
				vp_x1 = MIN(vp_x, vp_x1);
				vp_y1 = MIN(vp_y, vp_y1);
				vp_x2 = MAX(vp_x, vp_x2);
				vp_y2 = MAX(vp_y, vp_y2);
			}
		}

		/* Get viewport dimensions */
		glGetIntegerv( GL_VIEWPORT, viewport );
		width = viewport[2];
		height = viewport[3];

		/* Allow a 4-pixel margin of safety */
		vp_x1 = MAX(0, vp_x1 - 4);
		vp_y1 = MAX(0, vp_y1 - 4);
		vp_x2 = MIN(width - 1, vp_x2 + 4);
		vp_y2 = MIN(height - 1, vp_y2 + 4);

		/* Copy affected portion of front buffer to back buffer */
		glMatrixMode( GL_PROJECTION );
		glPushMatrix( ); /* Save for upcoming highlight draw */
		glLoadIdentity( );
		glOrtho( 0.0, (double)width, 0.0, (double)height, -1.0, 1.0 );
		glGetDoublev( GL_PROJECTION_MATRIX, prev_proj_matrix );
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix( ); /* Save for upcoming highlight draw */
		glLoadIdentity( );
		glRasterPos2i( vp_x1, vp_y1 );
		glGetDoublev( GL_MODELVIEW_MATRIX, prev_mview_matrix );

		glReadBuffer( GL_FRONT );
		glCopyPixels( vp_x1, vp_y1, vp_x2, vp_y2, GL_COLOR );

		prev_vp_x1 = vp_x1;
		prev_vp_y1 = vp_y1;
		prev_vp_x2 = vp_x2;
		prev_vp_y2 = vp_y2;

		/* Restore matrices for upcoming highlight draw */
		glMatrixMode( GL_PROJECTION );
		glPopMatrix( );
		glMatrixMode( GL_MODELVIEW );
		glPopMatrix( );
	}

	/* Draw highlight directly to front buffer */
	glDrawBuffer( GL_FRONT );

	/* Draw highlight */
	if (strong) {
		glLineWidth( 7.0 );
		glColor3f( 1.0, 0.75, 0.0 );
		draw_node( node );
		glColor3f( 1.0, 0.5, 0.0 );
	}
	else
		glColor3f( 1.0, 1.0, 1.0 );
	glLineWidth( 3.0 );
	draw_node( node );
	glLineWidth( 1.0 );

	/* Restore normal GL state */
	glDrawBuffer( GL_BACK );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glEnable( GL_LIGHTING );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_DITHER );

	glFlush( );

	highlight_drawn = TRUE;
	highlight_strong = strong;
	highlighted_node = node;
}


/* Frees all allocated display lists in the subtree rooted at the
 * specified directory node */
void
geometry_free_recursive( GNode *dnode )
{
	DirNodeDesc *dir_ndesc;
	GNode *node;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	dir_ndesc = DIR_NODE_DESC(dnode);

	if (dir_ndesc->a_dlist != NULL_DLIST) {
		glDeleteLists( dir_ndesc->a_dlist, 1 );
		dir_ndesc->a_dlist = NULL_DLIST;
	}
	if (dir_ndesc->b_dlist != NULL_DLIST) {
		glDeleteLists( dir_ndesc->b_dlist, 1 );
		dir_ndesc->b_dlist = NULL_DLIST;
	}
	if (dir_ndesc->c_dlist != NULL_DLIST) {
		glDeleteLists( dir_ndesc->c_dlist, 1 );
		dir_ndesc->c_dlist = NULL_DLIST;
	}

	/* Recurse into subdirectories */
	node = dnode->children;
	while (node != NULL) {
		if (NODE_IS_DIR(node))
			geometry_free_recursive( node );
		else
			break;
		node = node->next;
	}
}


/* end geometry.c */
