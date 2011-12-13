/* dirtree.h */

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


#ifdef FSV_DIRTREE_H
	#error
#endif
#define FSV_DIRTREE_H


#ifdef __GTK_H__
void dirtree_pass_widget( GtkWidget *ctree_w );
#endif
void dirtree_clear( void );
void dirtree_entry_new( GNode *dnode );
void dirtree_no_more_entries( void );
void dirtree_entry_show( GNode *dnode );
boolean dirtree_entry_expanded( GNode *dnode );
void dirtree_entry_collapse_recursive( GNode *dnode );
void dirtree_entry_expand( GNode *dnode );
void dirtree_entry_expand_recursive( GNode *dnode );


/* end dirtree.h */
