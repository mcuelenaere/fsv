/* color.c */

/* Node coloration */

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
#include "color.h"

#include <fnmatch.h>
#include <time.h>
#include "nvstore.h"

#include "animation.h" /* redraw( ) */
#include "geometry.h"
#include "window.h"


/* Some fnmatch headers don't define FNM_FILE_NAME */
/* (*cough*Solaris*cough*) */
#ifndef FNM_FILE_NAME
	#define FNM_FILE_NAME FNM_PATHNAME
#endif

/* Number of shades in a spectrum */
#define SPECTRUM_NUM_SHADES 1024


/* Default configuration */
static const ColorMode default_color_mode = COLOR_BY_NODETYPE;
static const char *default_nodetype_colors[NUM_NODE_TYPES] = {
	NULL,		/* Metanode (not used) */
	"#A0A0A0",	/* Directory */
	"#FFFFA0",	/* Regular file */
	"#FFFFFF",	/* Symlink */
	"#00FF00",	/* FIFO */
	"#FF8000",	/* Socket */
	"#00FFFF",	/* Character device */
	"#4CA0FF",	/* Block device */
	"#FF0000"	/* unknown */
};
static const int default_timestamp_spectrum_type = SPECTRUM_RAINBOW;
static const int default_timestamp_timestamp_type = TIMESTAMP_MODIFY;
static const int default_timestamp_period = 7 * 24 * 60 * 60; /* 1 week */
static const char default_timestamp_old_color[] = "#0000FF";
static const char default_timestamp_new_color[] = "#FF0000";
static const char *default_wpattern_groups[] = {
	"#FF3333", "*.arj", "*.gz", "*.lzh", "*.tar", "*.tgz", "*.z", "*.zip", "*.Z", NULL,
	"#FF33FF", "*.gif", "*.jpg", "*.png", "*.ppm", "*.tga", "*.tif", "*.xpm", NULL,
	"#FFFFFF", "*.au", "*.mov", "*.mp3", "*.mpg", "*.wav", NULL,
	NULL
};
static const char default_wpattern_default_color[] = "#FFFFA0";

/* For configuration file: key and token strings */
static const char key_color[] = "color";
static const char key_color_mode[] = "colormode";
static const char *tokens_color_mode[] = {
	"nodetype",
	"time",
	"wpattern",
	NULL
};
static const char key_nodetype[] = "nodetype";
static const char *keys_nodetype_node_type[NUM_NODE_TYPES] = {
	NULL,
	"directory",
	"regularfile",
	"symlink",
	"pipe",
	"socket",
	"chardevice",
	"blockdevice",
	"unknown"
};
static const char key_timestamp[] = "timestamp";
static const char key_timestamp_spectrum_type[] = "spectrumtype";
static const char *tokens_timestamp_spectrum_type[] = {
	"rainbow",
	"heat",
	"gradient",
	NULL
};
static const char key_timestamp_timestamp_type[] = "timestamptype";
static const char *tokens_timestamp_timestamp_type[] = {
	"access",
	"modify",
	"attribchange",
	NULL
};
static const char key_timestamp_period[] = "period";
static const char key_timestamp_old_color[] = "oldcolor";
static const char key_timestamp_new_color[] = "newcolor";
static const char key_wpattern[] = "wpattern";
static const char key_wpattern_group[] = "group";
static const char key_wpattern_group_color[] = "color";
static const char key_wpattern_group_wpattern[] = "wp";
static const char key_wpattern_default_color[] = "defaultcolor";

/* Color configuration */
static struct ColorConfig color_config;

/* Color assignment mode */
static ColorMode color_mode;

/* Colors for spectrum */
static RGBcolor spectrum_underflow_color;
static RGBcolor spectrum_colors[SPECTRUM_NUM_SHADES];
static RGBcolor spectrum_overflow_color;


/* Copies a ColorConfig structure from one location to another */
static void
color_config_copy( struct ColorConfig *to, struct ColorConfig *from )
{
	struct WPatternGroup *wpgroup, *new_wpgroup;
	GList *wpgroup_llink, *wp_llink;
	char *wpattern;

	/* Copy ColorByNodeType configuration */
	to->by_nodetype = from->by_nodetype; /* struct assign */

	/* Copy ColorByTime configuration */
	to->by_timestamp = from->by_timestamp; /* struct assign */

	/* Copy ColorByWPattern configuration */
	to->by_wpattern = from->by_wpattern; /* struct assign */
	to->by_wpattern.wpgroup_list = NULL;
	wpgroup_llink = from->by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;
		new_wpgroup = NEW(struct WPatternGroup);
		*new_wpgroup = *wpgroup; /* struct assign */
		G_LIST_APPEND(to->by_wpattern.wpgroup_list, new_wpgroup);

		new_wpgroup->wp_list = NULL;
		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;
			G_LIST_APPEND(new_wpgroup->wp_list, xstrdup( wpattern ));
			wp_llink = wp_llink->next;
		}

		wpgroup_llink = wpgroup_llink->next;
	}
}


/* Destructor for a ColorConfig structure. This frees everything except
 * the main structure itself (so that this can be used to flush out a
 * statically allocated struct) */
void
color_config_destroy( struct ColorConfig *ccfg )
{
	struct WPatternGroup *wpgroup;
	GList *wpgroup_llink, *wp_llink;
	char *wpattern;

	wpgroup_llink = ccfg->by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;

		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;
			xfree( wpattern );
			wp_llink = wp_llink->next;
		}
		g_list_free( wpgroup->wp_list );

		xfree( wpgroup );
		wpgroup_llink = wpgroup_llink->next;
	}
	g_list_free( ccfg->by_wpattern.wpgroup_list );
}


ColorMode
color_get_mode( void )
{
	return color_mode;
}


/* Returns (a copy of) the current color configuration. Note: It is the
 * responsibility of the caller to call color_config_destroy( ) on the
 * returned copy when it is no longer needed */
void
color_get_config( struct ColorConfig *ccfg )
{
	color_config_copy( ccfg, &color_config );
}


/* Returns the appropriate color for the given node, as per its type */
static const RGBcolor *
node_type_color( GNode *node )
{
	return &color_config.by_nodetype.colors[NODE_DESC(node)->type];
}


/* Returns the appropriate color for the given node, as per its timestamp */
static const RGBcolor *
time_color( GNode *node )
{
	double x;
        time_t node_time;
	int i;

	/* Directory override */
	if (NODE_IS_DIR(node))
		return node_type_color( node );

	/* Choose appropriate timestamp */
	switch (color_config.by_timestamp.timestamp_type) {
		case TIMESTAMP_ACCESS:
		node_time = NODE_DESC(node)->atime;
		break;

		case TIMESTAMP_MODIFY:
		node_time = NODE_DESC(node)->mtime;
		break;

		case TIMESTAMP_ATTRIB:
		node_time = NODE_DESC(node)->ctime;
		break;

		SWITCH_FAIL
	}

	/* Temporal position value (0 = old, 1 = new) */
	x = difftime( node_time, color_config.by_timestamp.old_time ) / difftime( color_config.by_timestamp.new_time, color_config.by_timestamp.old_time );

	if (x < 0.0) {
		/* Node is off the spectrum (too old) */
		return &spectrum_underflow_color;
	}

	if (x > 1.0) {
		/* Node is off the spectrum (too new) */
		return &spectrum_overflow_color;
	}

	/* Return a color somewhere in the spectrum */
	i = (int)floor( x * (double)(SPECTRUM_NUM_SHADES - 1) );
	return &spectrum_colors[i];
}


/* Returns the appropriate color for the given node, as matched (or not
 * matched) to the current set of wildcard patterns */
static const RGBcolor *
wpattern_color( GNode *node )
{
	struct WPatternGroup *wpgroup;
	GList *wpgroup_llink, *wp_llink;
	const char *name, *wpattern;

	/* Directory override */
	if (NODE_IS_DIR(node))
		return node_type_color( node );

	name = NODE_DESC(node)->name;

	/* Search for a match in the wildcard pattern groups */
	wpgroup_llink = color_config.by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;

		/* Check against patterns in this group */
		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;
			if (!fnmatch( wpattern, name, FNM_FILE_NAME | FNM_PERIOD ))
				return &wpgroup->color; /* A match! */
			wp_llink = wp_llink->next;
		}

		wpgroup_llink = wpgroup_llink->next;
	}

	/* No match */
	return &color_config.by_wpattern.default_color;
}


/* (Re)assigns colors to all nodes rooted at the given node */
void
color_assign_recursive( GNode *dnode )
{
	GNode *node;
	const RGBcolor *color;

	g_assert( NODE_IS_DIR(dnode) || NODE_IS_METANODE(dnode) );

	geometry_queue_rebuild( dnode );

	node = dnode->children;
	while (node != NULL) {
		switch (color_mode) {
			case COLOR_BY_NODETYPE:
			color = node_type_color( node );
			break;

			case COLOR_BY_TIMESTAMP:
			color = time_color( node );
			break;

			case COLOR_BY_WPATTERN:
			color = wpattern_color( node );
			break;

			SWITCH_FAIL
		}
                NODE_DESC(node)->color = color;

		if (NODE_IS_DIR(node))
			color_assign_recursive( node );

		node = node->next;
	}
}


/* Changes the current color mode */
void
color_set_mode( ColorMode mode )
{
	color_mode = mode;
	color_assign_recursive( globals.fstree );
	redraw( );
}


/* Returns a color in the given type of spectrum, at the given position
 * x = [0, 1]. If the spectrum is of a type which requires parameters,
 * those are passed in via the data argument */
RGBcolor
color_spectrum_color( SpectrumType type, double x, void *data )
{
	RGBcolor color;
	RGBcolor *zero_color, *one_color;

	g_assert( (x >= 0.0) && (x <= 1.0) );

	switch (type) {
		case SPECTRUM_RAINBOW:
		return rainbow_color( 1.0 - x );

		case SPECTRUM_HEAT:
		return heat_color( x );

		case SPECTRUM_GRADIENT:
		zero_color = ((RGBcolor **)data)[0];
		one_color = ((RGBcolor **)data)[1];
		color.r = zero_color->r + x * (one_color->r - zero_color->r);
		color.g = zero_color->g + x * (one_color->g - zero_color->g);
		color.b = zero_color->b + x * (one_color->b - zero_color->b);
		return color;

		SWITCH_FAIL
	}

	/* cc: duh... shouldn't there be a return value here? */
	color.r = -1.0; color.g = -1.0; color.b = -1.0; return color;
}


/* This sets up the spectrum color array */
static void
generate_spectrum_colors( void )
{
	RGBcolor *boundary_colors[2];
        double x;
	int i;
	void *data = NULL;

	if (color_config.by_timestamp.spectrum_type == SPECTRUM_GRADIENT) {
		boundary_colors[0] = &color_config.by_timestamp.old_color;
		boundary_colors[1] = &color_config.by_timestamp.new_color;
		data = boundary_colors;
	}

	for (i = 0; i < SPECTRUM_NUM_SHADES; i++) {
		x = (double)i / (double)(SPECTRUM_NUM_SHADES - 1);
		spectrum_colors[i] = color_spectrum_color( color_config.by_timestamp.spectrum_type, x, data ); /* struct assign */
	}

        /* Off-spectrum colors - make them dark */

	spectrum_underflow_color = spectrum_colors[0]; /* struct assign */
	spectrum_underflow_color.r *= 0.5;
	spectrum_underflow_color.g *= 0.5;
	spectrum_underflow_color.b *= 0.5;

	spectrum_overflow_color = spectrum_colors[(SPECTRUM_NUM_SHADES - 1)]; /* struct assign */
	spectrum_overflow_color.r *= 0.5;
	spectrum_overflow_color.g *= 0.5;
	spectrum_overflow_color.b *= 0.5;
}


/* Changes the current color configuration, and if mode is not COLOR_NONE,
 * sets the color mode as well */
void
color_set_config( struct ColorConfig *new_ccfg, ColorMode mode )
{
	color_config_destroy( &color_config );
	color_config_copy( &color_config, new_ccfg );

	generate_spectrum_colors( );

	if (globals.fsv_mode == FSV_SPLASH) {
		g_assert( mode != COLOR_NONE );
		color_mode = mode;
	}
	else if (mode != COLOR_NONE)
		color_set_mode( mode );
	else
		color_set_mode( color_mode );
}


/* Reads color configuration from file */
static void
color_read_config( void )
{
	struct WPatternGroup *wpgroup;
	NVStore *fsvrc;
	int i, x;
	char *str;

	fsvrc = nvs_open( CONFIG_FILE );

	nvs_change_path( fsvrc, key_color );

	/* Color mode */
	x = nvs_read_int_token_default( fsvrc, "mode", tokens_color_mode, default_color_mode );
	color_mode = (ColorMode)x;

	/* ColorByNodeType configuration */
	nvs_change_path( fsvrc, key_nodetype );
	for (i = 1; i < NUM_NODE_TYPES; i++) {
		str = nvs_read_string_default( fsvrc, keys_nodetype_node_type[i], default_nodetype_colors[i] );
		color_config.by_nodetype.colors[i] = hex2rgb( str ); /* struct assign */
		free( str ); /* !xfree */
	}
	nvs_change_path( fsvrc, ".." );

	/* ColorByTime configuration */
	nvs_change_path( fsvrc, key_timestamp );
	x = nvs_read_int_token_default( fsvrc, key_timestamp_spectrum_type, tokens_timestamp_spectrum_type, default_timestamp_spectrum_type );
	color_config.by_timestamp.spectrum_type = (SpectrumType)x;
	x = nvs_read_int_token_default( fsvrc, key_timestamp_timestamp_type, tokens_timestamp_timestamp_type, default_timestamp_timestamp_type );
	color_config.by_timestamp.timestamp_type = (TimeStampType)x;
	x = nvs_read_int_default( fsvrc, key_timestamp_period, default_timestamp_period );
	color_config.by_timestamp.new_time = time( NULL );
	color_config.by_timestamp.old_time = color_config.by_timestamp.new_time - (time_t)x;
	str = nvs_read_string_default( fsvrc, key_timestamp_old_color, default_timestamp_old_color );
	color_config.by_timestamp.old_color = hex2rgb( str ); /* struct assign */
	free( str ); /* !xfree */
	str = nvs_read_string_default( fsvrc, key_timestamp_new_color, default_timestamp_new_color );
	color_config.by_timestamp.new_color = hex2rgb( str ); /* struct assign */
	free( str ); /* !xfree */
	nvs_change_path( fsvrc, ".." );

	/* ColorByWPattern configuration */
	nvs_change_path( fsvrc, key_wpattern );
	/* Wildcard pattern groups */
	color_config.by_wpattern.wpgroup_list = NULL;
	nvs_vector_begin( fsvrc );
	while (nvs_path_present( fsvrc, key_wpattern_group )) {
		nvs_change_path( fsvrc, key_wpattern_group );

		wpgroup = NEW(struct WPatternGroup);
		str = nvs_read_string( fsvrc, key_wpattern_group_color );
		wpgroup->color = hex2rgb( str );
		free( str ); /* !xfree */

		/* Read in patterns */
		wpgroup->wp_list = NULL;
		nvs_vector_begin( fsvrc );
		while (nvs_path_present( fsvrc, key_wpattern_group_wpattern )) {
			str = nvs_read_string( fsvrc, key_wpattern_group_wpattern );
			G_LIST_APPEND(wpgroup->wp_list, xstrdup( str ));
			free( str ); /* !xfree */
		}
		nvs_vector_end( fsvrc );

		G_LIST_APPEND(color_config.by_wpattern.wpgroup_list, wpgroup);

		nvs_change_path( fsvrc, ".." );
	}
	/* Default color */
	str = nvs_read_string_default( fsvrc, key_wpattern_default_color, default_wpattern_default_color );
	color_config.by_wpattern.default_color = hex2rgb( str ); /* struct assign */
	free( str ); /* !xfree */
	nvs_change_path( fsvrc, ".." );

	nvs_change_path( fsvrc, ".." );

	nvs_close( fsvrc );
}


/* Writes color configuration to file */
void
color_write_config( void )
{
	struct WPatternGroup *wpgroup;
	NVStore *fsvrc;
	GList *wpgroup_llink, *wp_llink;
	int i;
	char *wpattern;

	fsvrc = nvs_open( CONFIG_FILE );

	/* Clean out existing color configuration information */
	nvs_change_path( fsvrc, key_color );
	nvs_delete_recursive( fsvrc, "." );

	/* Color mode */
	nvs_write_int_token( fsvrc, key_color_mode, color_mode, tokens_color_mode );

	/* ColorByNodeType configuration */
	nvs_change_path( fsvrc, key_nodetype );
	for (i = 1; i < NUM_NODE_TYPES; i++)
		nvs_write_string( fsvrc, keys_nodetype_node_type[i], rgb2hex( &color_config.by_nodetype.colors[i] ) );
	nvs_change_path( fsvrc, ".." );

	/* ColorByTime configuration */
	nvs_change_path( fsvrc, key_timestamp );
	nvs_write_int_token( fsvrc, key_timestamp_spectrum_type, color_config.by_timestamp.spectrum_type, tokens_timestamp_spectrum_type );
	nvs_write_int_token( fsvrc, key_timestamp_timestamp_type, color_config.by_timestamp.timestamp_type, tokens_timestamp_timestamp_type );
	nvs_write_int( fsvrc, key_timestamp_period, (int)difftime( color_config.by_timestamp.new_time, color_config.by_timestamp.old_time ) );
	nvs_write_string( fsvrc, key_timestamp_old_color, rgb2hex( &color_config.by_timestamp.old_color ) );
	nvs_write_string( fsvrc, key_timestamp_new_color, rgb2hex( &color_config.by_timestamp.new_color ) );
	nvs_change_path( fsvrc, ".." );

	/* ColorByWPattern configuration */
	nvs_change_path( fsvrc, key_wpattern );
	nvs_vector_begin( fsvrc );
	wpgroup_llink = color_config.by_wpattern.wpgroup_list;
	while (wpgroup_llink != NULL) {
		wpgroup = (struct WPatternGroup *)wpgroup_llink->data;

		nvs_change_path( fsvrc, key_wpattern_group );
		nvs_write_string( fsvrc, key_wpattern_group_color, rgb2hex( &wpgroup->color ) );

		nvs_vector_begin( fsvrc );
		wp_llink = wpgroup->wp_list;
		while (wp_llink != NULL) {
			wpattern = (char *)wp_llink->data;
			nvs_write_string( fsvrc, key_wpattern_group_wpattern, wpattern );
			wp_llink = wp_llink->next;
		}
		nvs_vector_end( fsvrc );

		nvs_change_path( fsvrc, ".." );

		wpgroup_llink = wpgroup_llink->next;
	}
	nvs_vector_end( fsvrc );
	nvs_write_string( fsvrc, key_wpattern_default_color, rgb2hex( &color_config.by_wpattern.default_color ) );
	nvs_change_path( fsvrc, ".." );

	nvs_close( fsvrc );
}


/* First-time initialization */
void
color_init( void )
{
	/* Read configuration file */
	color_read_config( );

	/* Update radio menu in window with configured color mode */
	window_set_color_mode( color_mode );

	/* Generate spectrum color table */
	generate_spectrum_colors( );
}


/* end color.c */
