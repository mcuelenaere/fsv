/* tmaptext.h */

/* Texture-mapped text */

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


#ifdef FSV_TMAPTEXT_H
	#error
#endif
#define FSV_TMAPTEXT_H


void text_init( void );
void text_pre( void );
void text_post( void );
void text_draw_straight( const char *text, const XYZvec *text_pos, const XYvec *text_max_dims );
void text_draw_straight_rotated( const char *text, const RTZvec *text_pos, const XYvec *text_max_dims );
void text_draw_curved( const char *text, const RTZvec *text_pos, const RTvec *text_max_dims );


/* end tmaptext.h */
