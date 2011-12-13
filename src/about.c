/* about.c */

/* Help -> About... */

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
#include "about.h"

#include <GL/gl.h>

#include "animation.h"
#include "geometry.h"
#include "ogl.h"
#include "tmaptext.h"


/* Interval normalization macro */
#define INTERVAL_PART(x,x0,x1)	(((x) - (x0)) / ((x1) - (x0)))


/* Normalized time variable (in range [0, 1]) */
static double about_part;

/* Display list for "fsv" geometry */
static GLuint fsv_dlist = NULL_DLIST;

/* TRUE while giving About presentation */
static boolean about_active = FALSE;


/* Draws the "fsv" 3D letters */
static void
draw_fsv( void )
{
	double dy, p, q;

	if (about_part < 0.5) {
		/* Set up a black, all-encompassing fog */
		glEnable( GL_FOG );
		glFogi( GL_FOG_MODE, GL_LINEAR );
		glFogf( GL_FOG_START, 200.0 );
		glFogf( GL_FOG_END, 1800.0 );
	}

	/* Set up projection matrix */
	glMatrixMode( GL_PROJECTION );
	glPushMatrix( );
	glLoadIdentity( );
	dy = 80.0 / ogl_aspect_ratio( );
	glFrustum( - 80.0, 80.0, - dy, dy, 80.0, 2000.0 );

	/* Set up modelview matrix */
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix( );
	glLoadIdentity( );
	if (about_part < 0.5) {
		/* Spinning and approaching fast */
		p = INTERVAL_PART(about_part, 0.0, 0.5);
		q = pow( 1.0 - p, 1.5 );
		glTranslated( 0.0, 0.0, -150.0 - 1800.0 * q );
		glRotated( 900.0 * q, 0.0, 1.0, 0.0 );
	}
	else if (about_part < 0.625) {
		/* Holding still for a moment */
		glTranslated( 0.0, 0.0, -150.0 );
	}
	else if (about_part < 0.75) {
		/* Flipping up and back */
		p = INTERVAL_PART(about_part, 0.625, 0.75);
		q = 1.0 - SQR(1.0 - p);
		glTranslated( 0.0, 40.0 * q, -150.0 - 50.0 * q );
		glRotated( 365.0 * q, 1.0, 0.0, 0.0 );
	}
	else {
		/* Holding still again */
		glTranslated( 0.0, 40.0, -200.0 );
		glRotated( 5.0, 1.0, 0.0, 0.0 );
	}

	/* Draw "fsv" geometry, using a display list if possible */
	if (fsv_dlist == NULL_DLIST) {
		fsv_dlist = glGenLists( 1 );
		glNewList( fsv_dlist, GL_COMPILE_AND_EXECUTE );
		geometry_gldraw_fsv( );
		glEndList( );
	}
	else
		glCallList( fsv_dlist );

	/* Restore previous matrices */
	glMatrixMode( GL_PROJECTION );
	glPopMatrix( );
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix( );

	glDisable( GL_FOG );
}


/* Draws the lines of text */
static void
draw_text( void )
{
        XYZvec tpos;
	XYvec tdims;
	double dy, p, q;

	if (about_part < 0.625)
		return;

	/* Set up projection matrix */
	glMatrixMode( GL_PROJECTION );
	glPushMatrix( );
	glLoadIdentity( );
	dy = 1.0 / ogl_aspect_ratio( );
	glFrustum( - 1.0, 1.0, - dy, dy, 1.0, 205.0 );

	/* Set up modelview matrix */
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix( );
	glLoadIdentity( );

        if (about_part < 0.75)
		p = INTERVAL_PART(about_part, 0.625, 0.75);
	else
		p = 1.0;
	q = (1.0 - SQR(1.0 - p));

	text_pre( );

	tdims.x = 400.0;
	tdims.y = 18.0;
	tpos.x = 0.0;
	tpos.y = -35.0; /* -35 */
	tpos.z = -200.0 * q;
	glColor3f( 1.0, 1.0, 1.0 );
	text_draw_straight( "fsv - 3D File System Visualizer", &tpos, &tdims );

	tdims.y = 15.0;
	tpos.y = 40.0 * q - 95.0; /* -55 */
	text_draw_straight( "Version " VERSION, &tpos, &tdims );

	tdims.y = 12.0;
	tpos.y = 100.0 * q - 180.0; /* -80 */
	glColor3f( 0.5, 0.5, 0.5 );
	text_draw_straight( "Copyright (C)1999 by Daniel Richard G.", &tpos, &tdims );

	tpos.y = 140.0 * q - 235.0; /* -95 */
	text_draw_straight( "<skunk@mit.edu>", &tpos, &tdims );

	/* Finally, fade in the home page URL */
	if (about_part > 0.75) {
		tpos.y = -115.0;
		p = INTERVAL_PART(about_part, 0.75, 1.0);
		q = SQR(SQR(p));
		glColor3f( q, q, 0.0 );
		text_draw_straight( "http://fox.mit.edu/skunk/soft/fsv/", &tpos, &tdims );
		text_draw_straight( "__________________________________", &tpos, &tdims );
	}

	text_post( );

	/* Restore previous matrices */
	glMatrixMode( GL_PROJECTION );
	glPopMatrix( );
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix( );
}


/* Progress callback; keeps viewport updated during presentation */
static void
about_progress_cb( Morph *unused )
{
	globals.need_redraw = TRUE;
}


/* Control routine */
boolean
about( AboutMesg mesg )
{
	switch (mesg) {
		case ABOUT_BEGIN:
		/* Begin the presentation */
		morph_break( &about_part );
		about_part = 0.0;
		morph_full( &about_part, MORPH_LINEAR, 1.0, 8.0, about_progress_cb, about_progress_cb, NULL );
		about_active = TRUE;
		break;

		case ABOUT_END:
		if (!about_active)
			return FALSE;
		/* We now return you to your regularly scheduled program */
		morph_break( &about_part );
		if (fsv_dlist != NULL_DLIST) {
			glDeleteLists( fsv_dlist, 1 );
			fsv_dlist = NULL_DLIST;
		}
		redraw( );
		about_active = FALSE;
		return TRUE;

		case ABOUT_DRAW:
		/* Draw all presentation elements */
		draw_fsv( );
		draw_text( );
		break;

		case ABOUT_CHECK:
		/* Return current presentation status */
		return about_active;

		SWITCH_FAIL
	}

	return FALSE;
}


/* end about.c */
