/* colexp.c */

/* Collapse/expansion engine */

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
#include "colexp.h"

#include "animation.h"
#include "camera.h"
#include "dirtree.h"
#include "filelist.h"
#include "geometry.h"
#include "gui.h" /* gui_update( ) */


/* Duration of a single collapse/expansion (in seconds) */
#define DISCV_COLEXP_TIME	1.5
#define MAPV_COLEXP_TIME	0.375
#define TREEV_COLEXP_TIME	0.5


/* TRUE whenever the collapse/expand process affects the viewport's
 * scrollable area */
static boolean scrollbars_colexp_adjust;


/* This returns the number of collapsed directory levels above the
 * given directory */
static int
collapsed_depth( GNode *dnode )
{
	GNode *up_node;
        int depth = 0;

	up_node = dnode->parent;
	while (NODE_IS_DIR(up_node)) {
		if (!DIR_COLLAPSED(up_node))
			break;
		++depth;
		up_node = up_node->parent;
	}

	return depth;
}


/* This returns the maximum depth to which a certain directory's
 * subtree has been expanded. A return value of 0 is typical; 1 or
 * more means a recursive collapse would be necessary if collapsing
 * the given directory */
static int
max_expanded_depth( GNode *dnode )
{
	GNode *node;
	int max_depth = 0;
	int subtree_depth;

	node = dnode->children;
	while (node != NULL) {
		if (NODE_IS_DIR(node)) {
			if (DIR_EXPANDED(node))
				subtree_depth = 1 + max_expanded_depth( node );
			else
				subtree_depth = 0;
			max_depth = MAX(max_depth, subtree_depth);
		}
		else
			break;
		node = node->next;
	}

	return max_depth;
}


/* Step/end callback for collapses/expands */
static void
colexp_progress_cb( Morph *morph )
{
	GNode *dnode;

	dnode = (GNode *)morph->data;
	g_assert( NODE_IS_DIR(dnode) );

        /* Keep geometry module appraised of collapse/expand progress */
	geometry_colexp_in_progress( dnode );

	/* Keep viewport refreshed */
	globals.need_redraw = TRUE;

	if (scrollbars_colexp_adjust)
		camera_update_scrollbars( ABS(*(morph->var) - morph->end_value) < EPSILON );
}


/* This keeps the directory tree and the map geometry in sync
 * (expansion state vs. "deployment" value) */
void
colexp( GNode *dnode, ColExpMesg mesg )
{
	static double colexp_time;
	static int depth = 0;
	static int max_depth;
	GNode *node;
	double wait_time;
	double pan_time;
	int wait_count = 0;
	boolean curnode_is_ancestor, curnode_is_descendant, curnode_is_equal;

	g_assert( NODE_IS_DIR(dnode) );

	if (depth == 0) {
#ifdef DEBUG
		if (mesg != COLEXP_EXPAND_ANY) {
			/* All ancestor directories must be expanded */
			node = dnode->parent;
			while (NODE_IS_DIR(node)) {
				g_assert( DIR_NODE_DESC(node)->deployment > (1.0 - EPSILON) );
				node = node->parent;
			}
		}
#endif

		/* Update ctree and determine maximum recursion depth */
		switch (mesg) {
			case COLEXP_COLLAPSE_RECURSIVE:
			dirtree_entry_collapse_recursive( dnode );
			max_depth = max_expanded_depth( dnode );
			break;

			case COLEXP_EXPAND:
			dirtree_entry_expand( dnode );
			max_depth = 0;
			break;

			case COLEXP_EXPAND_ANY:
			dirtree_entry_expand( dnode );
			max_depth = collapsed_depth( dnode );
			break;

			case COLEXP_EXPAND_RECURSIVE:
			dirtree_entry_expand_recursive( dnode );
			/* max_depth will be used as a high-water mark */
			max_depth = 0;
			break;

			SWITCH_FAIL
		}

		/* Make file list appropriately (in)accessible */
		filelist_reset_access( );

		gui_update( );

		/* Collapse/expand time for current visualization mode */
		switch (globals.fsv_mode) {
			case FSV_DISCV:
			colexp_time = DISCV_COLEXP_TIME;
			break;

			case FSV_MAPV:
			colexp_time = MAPV_COLEXP_TIME;
			break;

			case FSV_TREEV:
			colexp_time = TREEV_COLEXP_TIME;
			break;

                        SWITCH_FAIL
		}
	}

	morph_break( &DIR_NODE_DESC(dnode)->deployment );

	/* Determine time to wait before collapsing/expanding directory */
	switch (mesg) {
		case COLEXP_COLLAPSE_RECURSIVE:
		wait_count = max_depth - depth;
		break;

		case COLEXP_EXPAND_RECURSIVE:
		case COLEXP_EXPAND:
		wait_count = depth;
		break;

		case COLEXP_EXPAND_ANY:
		wait_count = max_depth - depth;
		break;

		SWITCH_FAIL
	}
	if (wait_count > 0) {
		wait_time = (double)wait_count * colexp_time;
		morph( &DIR_NODE_DESC(dnode)->deployment, MORPH_LINEAR, DIR_NODE_DESC(dnode)->deployment, wait_time );
	}

	/* Initiate collapse/expand */
	switch (mesg) {
		case COLEXP_COLLAPSE_RECURSIVE:
		morph_full( &DIR_NODE_DESC(dnode)->deployment, MORPH_QUADRATIC, 0.0, colexp_time, colexp_progress_cb, colexp_progress_cb, dnode );
		break;

		case COLEXP_EXPAND:
		case COLEXP_EXPAND_ANY:
		case COLEXP_EXPAND_RECURSIVE:
		morph_full( &DIR_NODE_DESC(dnode)->deployment, MORPH_INV_QUADRATIC, 1.0, colexp_time, colexp_progress_cb, colexp_progress_cb, dnode );
		break;

		SWITCH_FAIL
	}

	/* Recursion */
	/* geometry_colexp_initiated( ) is called at differing points below
	 * because (at least in TreeV mode) notification must always
	 * proceed from parent to children, and not the other way around */
	switch (mesg) {
		case COLEXP_EXPAND:
		/* Initial collapse/expand notify */
		geometry_colexp_initiated( dnode );
		/* EXPAND does not walk the tree */
		break;

		case COLEXP_EXPAND_ANY:
		/* Ensure that all parent directories are expanded */
		if (NODE_IS_DIR(dnode->parent)) {
			++depth;
			colexp( dnode->parent, COLEXP_EXPAND_ANY );
			--depth;
		}
		/* Initial collapse/expand notify */
		geometry_colexp_initiated( dnode );
		break;

		case COLEXP_COLLAPSE_RECURSIVE:
		case COLEXP_EXPAND_RECURSIVE:
		/* Initial collapse/expand notify */
		geometry_colexp_initiated( dnode );
		/* Perform action on subdirectories */
		++depth;
		node = dnode->children;
		while (node != NULL) {
			if (NODE_IS_DIR(node))
				colexp( node, mesg );
			else
				break;
			node = node->next;
		}
		--depth;
		break;

		SWITCH_FAIL
	}

	if (mesg == COLEXP_EXPAND_RECURSIVE) {
		/* Update high-water mark */
		max_depth = MAX(max_depth, depth);
	}

	if (depth == 0) {
		/* Determine position of current node w.r.t. the
		 * collapsing/expanding directory node */
		curnode_is_ancestor = g_node_is_ancestor( globals.current_node, dnode );
		curnode_is_equal = globals.current_node == dnode;
		curnode_is_descendant = g_node_is_ancestor( dnode, globals.current_node );

		/* Handle the camera semi-intelligently if it is not under
		 * manual control */
		if (!camera->manual_control) {
			switch (mesg) {
				case COLEXP_COLLAPSE_RECURSIVE:
				pan_time = (double)(max_depth + 1) * colexp_time;
				if (curnode_is_ancestor || curnode_is_equal)
					camera_look_at_full( globals.current_node, MORPH_LINEAR, pan_time );
				else if (curnode_is_descendant)
					camera_look_at_full( dnode, MORPH_LINEAR, pan_time );
				break;

				case COLEXP_EXPAND:
				case COLEXP_EXPAND_RECURSIVE:
				if (curnode_is_ancestor || curnode_is_equal) {
					pan_time = (double)(max_depth + 1) * colexp_time;
					camera_look_at_full( globals.current_node, MORPH_LINEAR, pan_time );
				}
				break;

				case COLEXP_EXPAND_ANY:
				/* Don't do anything. Something else
				 * should already be doing something
				 * with the camera */
				break;

				SWITCH_FAIL
			}
		}

		/* If, in TreeV mode, the current node is an ancestor of
		 * a collapsing/expanding directory, the scrollbars may
		 * need updating to reflect a new scroll range */
		scrollbars_colexp_adjust = FALSE;
		if (curnode_is_ancestor && (globals.fsv_mode == FSV_TREEV))
			scrollbars_colexp_adjust = TRUE;
	}
}


/* end colexp.c */
