/* fsv.c */

/* Program entry */

/* fsv - 3D File System Visualizer
 *
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 */

/* This program is free software; you can redistribute it and/or
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
#include "fsv.h"

#include <gtk/gtk.h>
#include <gtkgl/gdkgl.h>
#include "getopt.h"

#include "about.h"
#include "animation.h"
#include "camera.h"
#include "color.h" /* color_init( ) */
#include "filelist.h"
#include "geometry.h"
#include "gui.h" /* gui_update( ) */
#include "scanfs.h"
#include "window.h"


/* Identifiers for command-line options */
enum {
	OPT_DISCV,
	OPT_MAPV,
	OPT_TREEV,
	OPT_CACHEDIR,
	OPT_NOCACHE,
	OPT_HELP
};


/* Initial visualization mode */
static FsvMode initial_fsv_mode = FSV_MAPV;

/* Command-line options */
static struct option cli_opts[] = {
	{ "discv", no_argument, NULL, OPT_DISCV },
	{ "mapv", no_argument, NULL, OPT_MAPV },
	{ "treev", no_argument, NULL, OPT_TREEV },
	{ "cachedir", required_argument, NULL, OPT_CACHEDIR },
	{ "nocache", no_argument, NULL, OPT_NOCACHE },
	{ "help", no_argument, NULL, OPT_HELP },
	{ NULL, 0, NULL, 0 }
};

/* Usage summary */
static const char usage_summary[] = __("\n"
    "fsv - 3D File System Visualizer\n"
    "      Version " VERSION "\n"
    "Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>\n"
    "\n"
    "Usage: %s [rootdir] [options]\n"
    "  rootdir      Root directory for visualization\n"
    "               (defaults to current directory)\n"
    "  --mapv       Start in MapV mode (default)\n"
    "  --treev      Start in TreeV mode\n"
    "  --help       Print this help and exit\n"
    "\n");


/* Helper function for fsv_set_mode( ) */
static void
initial_camera_pan( char *mesg )
{
	/* To prevent root_dnode from appearing twice in a row at
	 * the bottom of the node history stack */
	G_LIST_PREPEND(globals.history, NULL);

	if (!strcmp( mesg, "new_fs" )) {
		/* First look at new filesystem */
		camera_look_at_full( root_dnode, MORPH_SIGMOID, 4.0 );
	}
	else {
		/* Same filesystem, different visualization mode */
		if (globals.fsv_mode == FSV_TREEV) {
			/* Enter TreeV mode with an L-shaped pan */
			camera_treev_lpan_look_at( globals.current_node, 1.0 );
		}
		else
			camera_look_at_full( globals.current_node, MORPH_INV_QUADRATIC, 1.0 );
	}
}


/* Switches between visualization modes */
void
fsv_set_mode( FsvMode mode )
{
	boolean first_init = FALSE;

	switch (globals.fsv_mode) {
		case FSV_SPLASH:
		/* Queue desired mode */
		initial_fsv_mode = mode;
		return;

		case FSV_NONE:
		/* Filesystem's first appearance */
		first_init = TRUE;
		break;

		default:
		/* Set initial mode for next time */
		initial_fsv_mode = mode;
		break;
	}

	/* Generate appropriate visualization geometry */
	geometry_init( mode );

	/* Set up initial camera state */
	camera_init( mode, first_init );

	globals.fsv_mode = mode;

	/* Ensure that About presentation is not up */
	about( ABOUT_END );

	/* Render one frame before performing the initial camera pan.
	 * There are two separate reasons for doing this: */
	if (first_init) {
		/* 1. Practical limitations make the first frame take an
		 * unusually long time to render, so this avoids a really
		 * unpleasant camera jump */
		schedule_event( initial_camera_pan, "new_fs", 1 );
	}
	else {
		/* 2. In order to do a camera pan, the geometry needs to
		 * be defined. We just called geometry_init( ), but if the
		 * camera's going to a non-root node, it may very well not
		 * have been laid out yet (but it will be when drawn) */
		schedule_event( initial_camera_pan, "", 1 );
	}
}


/* Performs filesystem scan and first-time initialization */
void
fsv_load( const char *dir )
{
	/* Lock down interface */
	window_set_access( FALSE );

	/* Bring up splash screen */
	globals.fsv_mode = FSV_SPLASH;
	redraw( );

	/* Reset scrollbars (disable scrolling) */
	camera_update_scrollbars( TRUE );

	gui_update( );

	/* Scan filesystem */
	scanfs( dir );

	/* Clear/reset node history */
	g_list_free( globals.history );
	globals.history = NULL;
	globals.current_node = root_dnode;

	/* Initialize file list */
	filelist_init( );
	gui_update( );

	/* Initialize visualization */
	globals.fsv_mode = FSV_NONE;
	fsv_set_mode( initial_fsv_mode );
}


void
fsv_write_config( void )
{

/* #warning write fsv_write_config( ) */
#if 0
	/* Clean out old configuration information */
	gnome_config_push_prefix( config_path_prefix( NULL ) );
	gnome_config_clean_section( section_fsv );
	gnome_config_pop_prefix( );

	gnome_config_push_prefix( config_path_prefix( section_fsv ) );
	gnome_config_set_token( key_fsv_mode, globals.fsv_mode, tokens_fsv_mode );
	gnome_config_pop_prefix( );
#endif /* 0 */
}


int
main( int argc, char **argv )
{
	int opt_id;
	char *root_dir;

	/* Initialize global variables */
	globals.fstree = NULL;
	globals.history = NULL;
	/* Set sane camera state so setup_modelview_matrix( ) in ogl.c
	 * doesn't choke. (It does get called in splash screen mode) */
	camera->fov = 45.0;
	camera->near_clip = 1.0;
	camera->far_clip = 2.0;

#ifdef DEBUG
	debug_init( );
#endif
#ifdef ENABLE_NLS
	/* Initialize internationalization (i8e i18n :-) */
	setlocale( LC_ALL, "" );
	bindtextdomain( PACKAGE, LOCALEDIR );
	textdomain( PACKAGE );
#endif

	/* Parse command-line options */
	for (;;) {
		opt_id = getopt_long( argc, argv, "", cli_opts, NULL );
		if (opt_id < 0)
			break;
		switch (opt_id) {
			case OPT_DISCV:
			/* --discv */
			initial_fsv_mode = FSV_DISCV;
			break;

			case OPT_MAPV:
			/* --mapv */
			initial_fsv_mode = FSV_MAPV;
			break;

			case OPT_TREEV:
			/* --treev */
			initial_fsv_mode = FSV_TREEV;
			break;

			case OPT_CACHEDIR:
			/* --cachedir <dir> */
			printf( "cache directory: %s\n", optarg );
			printf( "(caching not yet implemented)\n" );
			/* TODO: Implement caching */
			break;

			case OPT_NOCACHE:
			/* --nocache */
			/* TODO: Implement caching */
			break;

			case OPT_HELP:
			/* --help */
			default:
			/* unrecognized option */
			printf( _(usage_summary), argv[0] );
			fflush( stdout );
			exit( EXIT_SUCCESS );
			break;
		}
	}

	/* Determine root directory */
	if (optind < argc) {
                /* From command line */
		root_dir = xstrdup( argv[optind++] );
		if (optind < argc) {
			/* Excess arguments! */
			fprintf( stderr, _("Junk in command line:") );
			while (optind < argc)
				fprintf( stderr, " %s", argv[optind++] );
			fprintf( stderr, "\n" );
			fflush( stderr );
		}
	}
	else {
		/* Use current directory */
		root_dir = xstrdup( "." );
	}

	/* Initialize GTK+ */
	gtk_init( &argc, &argv );

	/* Check for OpenGL support */
	if (!gdk_gl_query( ))
		quit( _("fsv requires OpenGL support.") );

	window_init( initial_fsv_mode );
	color_init( );

	fsv_load( root_dir );
	xfree( root_dir );

	gtk_main( );

	return 0;
}


/* end fsv.c */
