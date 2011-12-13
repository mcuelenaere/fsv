/* animation.c */

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


#include "common.h"
#include "animation.h"

#include <gtk/gtk.h>

#include "ogl.h" /* ogl_draw( ) */


/* The framerate is maintained as a rolling average over this
 * length of time (in seconds) */
#define FRAMERATE_AVERAGE_TIME 4.0


/* Messages for framerate_iteration( ) */
enum {
	FRAME_RENDERED,
	STOP_TIMING
};


/* Scheduled event queue */
static GList *schevent_queue = NULL;

/* Morph queue */
static GList *morph_queue = NULL;

/* Overall graphics framerate */
/* TODO: Export framerate when there's something to use it */
static float framerate = 0.0;

/* TRUE for as long as something somewhere is being animated */
static boolean animation_active = FALSE;


/* Schedules an event (callback) to occur after the given number of
 * frames have elapsed */
void
schedule_event( void (*event_cb)( ), void *data, int nframes )
{
	ScheduledEvent *new_schevent;

	new_schevent = NEW(ScheduledEvent);
	new_schevent->nframes = nframes;
	new_schevent->event_cb = event_cb;
	new_schevent->data = data;

	/* Make sure we're animating */
	if (!animation_active)
		redraw( );

	/* Add new scheduled event to queue */
	G_LIST_PREPEND(schevent_queue, new_schevent);
}


/* This executes scheduled events. Return status indicates if any events
 * are pending, or if an event was just executed */
static boolean
scheduled_event_iteration( void )
{
	ScheduledEvent *schevent;
	GList *seq_llink;
	boolean event_executed = FALSE;

	/* Update entries in queue, executing those that are
	 * scheduled for the current frame */
	seq_llink = schevent_queue;
	while (seq_llink != NULL) {
		schevent = (ScheduledEvent *)seq_llink->data;
		if (--schevent->nframes <= 0) {
			/* Execute event */
			(schevent->event_cb)( schevent->data );
			/* Remove and free record */
			seq_llink = seq_llink->next;
			G_LIST_REMOVE(schevent_queue, schevent);
			xfree( schevent );

			event_executed = TRUE;
		}
		else
			seq_llink = seq_llink->next;
	}

	return (event_executed || (schevent_queue != NULL));
}


/* Checks if the two given morphs are on the same variable */
static int
compare_var( const Morph *morph1, const Morph *morph2 )
{
	return morph1->var != morph2->var;
}


/* Helper function for morph_full( ). Returns the last stage of a
 * multi-staged morph (or just the argument, if single-staged) */
static Morph *
last_morph_stage( Morph *m )
{
	while (m->next != NULL)
		m = m->next;

	return m;
}


/* Initiates a morph on *var toward target_value. step_cb is called every
 * time the variable is updated, except for the final update, at which
 * time end_cb is called. Optionally, an arbitrary data pointer can be
 * passed in, which is accessible through the 'struct Morph *' passed to
 * the callback. Multi-stage morphs can be specified by calling this
 * repeatedly on the same variable */
void
morph_full( double *var, MorphType type, double target_value, double duration, void (*step_cb)( Morph * ), void (*end_cb)( Morph * ), void *data )
{
	Morph *new_morph, *morph;
	Morph *mlast;
	GList *mq_llink;
	double t_now;

	t_now = xgettime( );

	/* Create new morph record */
	new_morph = NEW(Morph);
	new_morph->type = type;
	new_morph->var = var;
	new_morph->start_value = *var;
	new_morph->end_value = target_value;
	new_morph->t_start = t_now;
	new_morph->t_end = t_now + duration;
	new_morph->step_cb = step_cb;
	new_morph->end_cb = end_cb;
	new_morph->data = data;
	new_morph->next = NULL;

	/* Check to see if the variable is already undergoing morphing */
	mq_llink = g_list_find_custom( morph_queue, new_morph, (GCompareFunc)compare_var );
	if (mq_llink == NULL) {
		/* Variable is not being morphed */
		/* Make sure we're animating */
		if (!animation_active)
			redraw( );
		/* Add new morph to queue */
		G_LIST_PREPEND(morph_queue, new_morph);
	}
	else {
		/* Variable is already undergoing morphing. Append
		 * new stage to the incumbent morph record(s) */
		morph = (Morph *)mq_llink->data;
		mlast = last_morph_stage( morph );
		new_morph->t_start = mlast->t_end;
		new_morph->t_end = mlast->t_end + duration;
		new_morph->start_value = mlast->end_value;
		mlast->next = new_morph;
	}
}


/* Calls morph_full( ) with defaults (no callbacks nor extra data) */
void
morph( double *var, MorphType type, double target_value, double duration )
{
	morph_full( var, type, target_value, duration, NULL, NULL, NULL );
}


/* Causes an ongoing morph to finish immediately. The variable is set to
 * its final value, and the end callback is called. If the given variable
 * is not being morphed, this will return silently */
void
morph_finish( double *var )
{
	Morph finish_morph, *morph;
	GList *mq_llink;

	finish_morph.var = var;
	mq_llink = g_list_find_custom( morph_queue, &finish_morph, (GCompareFunc)compare_var );
	if (mq_llink == NULL)
		return; /* Variable is not being morphed */

	morph = (Morph *)mq_llink->data;
	morph->t_end = 0.0;
}


/* Stops an ongoing morph on a variable. The value is not updated, and
 * neither of the step/end callbacks are called. If the given variable
 * is not being morphed, this will silently return */
void
morph_break( double *var )
{
	Morph break_morph, *morph, *mnext;
	GList *mq_llink;

	break_morph.var = var;
	mq_llink = g_list_find_custom( morph_queue, &break_morph, (GCompareFunc)compare_var );
	if (mq_llink == NULL)
		return; /* Variable is not being morphed */

	/* Remove morph record */
	morph = (Morph *)mq_llink->data;
	G_LIST_REMOVE(morph_queue, morph);

	/* Free morph record, and any subsequent stages */
	while (morph != NULL) {
		mnext = morph->next;
		xfree( morph );
		morph = mnext;
	}
}


/* Driver routine for variable morphing.
 * Return value indicates whether state change occurred or not */
static boolean
morph_iteration( void )
{
	Morph *morph;
	GList *mq_llink;
	double t_now;
	double percent;
	boolean state_changed = FALSE;

	t_now = xgettime( );

	/* Perform update of all morphing variables */
	mq_llink = morph_queue;
	while (mq_llink != NULL) {
		morph = (Morph *)mq_llink->data;

		if (t_now >= morph->t_end) {
			/* Morph complete - assign end value */
			*(morph->var) = morph->end_value;
			state_changed = TRUE;
			/* Call end callback, if there is one */
			if (morph->end_cb != NULL)
				(morph->end_cb)( morph );
			if (morph->next != NULL) {
				/* Drop in next stage */
				mq_llink->data = morph->next;
			}
			else {
				/* Remove record from queue */
				mq_llink = mq_llink->next;
				G_LIST_REMOVE(morph_queue, morph);
			}
			xfree( morph );
                        continue;
		}

		/* Update variable value using appropriate remapping */
		percent = (t_now - morph->t_start) / (morph->t_end - morph->t_start);
		switch (morph->type) {
			case MORPH_LINEAR:
			/* No remapping */
			break;

			case MORPH_QUADRATIC:
			/* Parabolic curve */
			percent = SQR(percent);
			break;

			case MORPH_INV_QUADRATIC:
			/* Inverted parabolic curve */
			percent = 1.0 - SQR(1.0 - percent);
			break;

			case MORPH_SIGMOID:
			/* Sigmoidal (S-like) remapping */
			percent = 0.5 * (1.0 - cos( PI * percent ));
			break;

			case MORPH_SIGMOID_ACCEL:
			/* Sigmoidal, with acceleration */
			percent = 0.5 * (1.0 - cos( PI * SQR(percent) ));
			break;

                        SWITCH_FAIL
		}

		/* Assign new variable value */
		*(morph->var) = INTERPOLATE(percent, morph->start_value, morph->end_value);
		state_changed = TRUE;

		/* Call step callback, if there is one */
		if (morph->step_cb != NULL)
			(morph->step_cb)( morph );

		mq_llink = mq_llink->next;
	}

	return state_changed;
}


/* Calculates framerate */
static void
framerate_iteration( int mesg )
{
	static double t_prev = -1.0;
        static double sum_frametimes = 0.0;
        static double *frametimes;
	static int num_frametimes = 0;
	static int f = 0;
	double t_now, delta_t;
	double average_frametime;
#ifdef DEBUG
	double sigma_sum = 0.0;
	int i;
#endif

	if (num_frametimes == 0) {
		/* First-time initialization */
		num_frametimes = 1;
		frametimes = NEW_ARRAY(double, num_frametimes);
		frametimes[0] = 0.0;
		return;
	}

	if (mesg == STOP_TIMING) {
		/* Entering steady state */
		t_prev = -1.0;
		return;
	}

	g_assert( mesg == FRAME_RENDERED );

        t_now = xgettime( );

	if (t_prev < 0.0) {
		/* First frame after steady state */
		t_prev = t_now;
		return;
	}

	/* Time for last rendered frame */
	delta_t = t_now - t_prev;

        /* Update frametime buffer and sum */
	sum_frametimes -= frametimes[f];
	frametimes[f] = delta_t;
	sum_frametimes += delta_t;

	/* Update framerate */
	average_frametime = sum_frametimes / (double)num_frametimes;
	framerate = 1.0 / average_frametime;

	/* Check that the frametime buffer isn't too small */
	if (sum_frametimes < FRAMERATE_AVERAGE_TIME) {
		/* Insert a new element into the frametime buffer, right
		 * after the current element, with value equal to that
		 * of the oldest element */
		++num_frametimes;
		RESIZE(frametimes, num_frametimes, double);
		if (f < (num_frametimes - 2))
			memmove( &frametimes[f + 2], &frametimes[f + 1], (num_frametimes - f - 2) * sizeof(double) );
		else
			frametimes[f + 1] = frametimes[0];
		sum_frametimes += frametimes[f + 1];
	}

	/* Check that the frametime buffer isn't too big */
	if ((sum_frametimes > (FRAMERATE_AVERAGE_TIME + 1.0)) && (num_frametimes > 4)) {
		/* Remove the oldest element from the frametime array */
		if (f < (num_frametimes - 1)) {
			sum_frametimes -= frametimes[f + 1];
			memmove( &frametimes[f + 1], &frametimes[f + 2], (num_frametimes - f - 2) * sizeof(double) );
		}
		else {
			sum_frametimes -= frametimes[0];
			memmove( &frametimes[0], &frametimes[1], (num_frametimes - 1) * sizeof(double) );
		}
		--num_frametimes;
		RESIZE(frametimes, num_frametimes, double);
	}

	f = (f + 1) % num_frametimes;

#ifdef DEBUG
        /* Check that sum_frametimes == SIGMA frametimes */
	for (i = 0; i < num_frametimes; i++)
		sigma_sum += frametimes[i];
	g_assert( ABS(sigma_sum - sum_frametimes) < 0.001 );
#endif
}


/* Top-level animation loop */
static boolean
animation_loop( void )
{
	boolean state_changed, schevents_pending = FALSE;

	/* Update morphing variables */
	state_changed = morph_iteration( );

	if (globals.need_redraw) {
		/* Redraw viewport */
		ogl_draw( );

		/* Update framerate */
		framerate_iteration( FRAME_RENDERED );

		/* Execute scheduled events */
		schevents_pending = scheduled_event_iteration( );

		if (!schevents_pending)
			globals.need_redraw = FALSE;
	}

	if (!state_changed && !schevents_pending) {
                /* Entering steady state */
		framerate_iteration( STOP_TIMING );
		animation_active = FALSE;
	}

	/* (returning FALSE terminates looping) */
	return animation_active;
}


/* This is the official means of requesting a redraw */
void
redraw( void )
{
	/* Ensure that animation loop is active */
	if (!animation_active)
		gtk_idle_add_priority( G_PRIORITY_LOW, (GtkFunction)animation_loop, NULL );

	animation_active = TRUE;
	globals.need_redraw = TRUE;
}


/* end animation.c */
