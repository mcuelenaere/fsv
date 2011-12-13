/* animation.h */

/* Animation control */

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


#ifdef FSV_ANIMATION_H
	#error
#endif
#define FSV_ANIMATION_H


typedef enum {
	MORPH_LINEAR,
	MORPH_QUADRATIC,
	MORPH_INV_QUADRATIC,
	MORPH_SIGMOID,
	MORPH_SIGMOID_ACCEL
} MorphType;

typedef struct _ScheduledEvent ScheduledEvent;
struct _ScheduledEvent {
	/* Wait for these many frames to elapse... */
	int	nframes;
	/* ...and then call this function... */
	void	(*event_cb)( void * );
	/* ...with this arbitrary data pointer. */
	void	*data;
};

typedef struct _Morph Morph;
struct _Morph {
	MorphType	type;		/* Type of morph */
	double		*var;		/* The variable */
	double		start_value;	/* Initial value for variable */
	double		end_value;	/* Target value for variable */
	double		t_start;	/* Starting time */
	double		t_end;		/* Ending time */
	/* Step callback is called after every incremental update
	 * except the last one */
	void		(*step_cb)( Morph * );
	/* End callback is called after the last update */
	void		(*end_cb)( Morph * );
	void		*data;		/* Arbitrary data pointer */
	Morph		*next;		/* Next morph (for chaining) */
};


void schedule_event( void (*event_cb)(  ), void *data, int nframes );
void morph_full( double *var, MorphType type, double target_value, double duration, void (*step_cb)( Morph * ), void (*end_cb)( Morph * ), void *data );
void morph( double *var, MorphType type, double target_value, double duration );
void morph_finish( double *var );
void morph_break( double *var );
void redraw( void );


/* end animation.h */
