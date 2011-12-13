/* filelist.h */

/* File list control */

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


#ifdef FSV_FILELIST_H
	#error
#endif
#define FSV_FILELIST_H


#ifdef __GTK_H__
void filelist_pass_widget( GtkWidget *clist_w );
GtkWidget *dir_contents_list( GNode *dnode );
#endif
void filelist_reset_access( void );
void filelist_populate( GNode *dnode );
void filelist_show_entry( GNode *node );
void filelist_init( void );
void filelist_scan_monitor_init( void );
void filelist_scan_monitor( int *node_counts, int64 *size_counts );


/* end filelist.h */
