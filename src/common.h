/* common.h */

/* Common header file */

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


#ifdef FSV_COMMON_H
	#error
#endif
#define FSV_COMMON_H


#define G_LOG_DOMAIN PACKAGE


/**** Headers ****************/

/* Autoconf */
#include "config.h"

/* System stuff */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>

/* GLib */
#include <glib.h>

/* Internationalization */
#ifdef ENABLE_NLS
	#include <locale.h>
	#include <libintl.h>
	#define _(msgid) gettext(msgid)
	#ifdef gettext_noop
		#define __(msgid) gettext_noop(msgid)
	#else
		#define __(msgid) msgid
	#endif
#else /* not ENABLE_NLS */
	#define _(string) string
	#define __(string) string
#endif /* not ENABLE_NLS */

/* Debugging */
#ifdef DEBUG
	#include "debug.h"
#else
	#define _xfree xfree
#endif


/**** Constants, macros, types ****************/

/* Configuration file (in user's home directory) */
#define CONFIG_FILE		"~/.fsvrc"

/* Mathematical constants et. al. */
#define LN_2			0.69314718055994530942
#define SQRT_2			1.41421356237309504880
#define MAGIC_NUMBER		1.61803398874989484821
#define PI			3.14159265358979323846
#define EPSILON			1.0e-6
#define NIL			0
#define NULL_DLIST		0

/* Alias for the root directory node */
#define root_dnode		globals.fstree->children

/* Mathematical macros */
#define SQR(x)			((x)*(x))
#define NRATIO(x,y)		(MIN(ABS(x),ABS(y)) / MAX(ABS(x),ABS(y)))
#define DEG(angleRad)		((angleRad) * (180.0 / PI))
#define RAD(angle360)		((angle360) * (PI / 180.0))
#define DOT_PROD(v1,v2)		(v1.x * v2.x + v1.y * v2.y + v1.z * v2.z)
#define XY_LEN(v)		hypot( v.x, v.y )
#define XYZ_LEN(v)		sqrt( SQR(v.x) + SQR(v.y) + SQR(v.z) )
#define INTERPOLATE(k,a,b)	((a) + (k)*((b) - (a)))

/* Convenience macros */
#define G_LIST_PREPEND(l,d)	l = g_list_prepend( l, d )
#define G_LIST_APPEND(l,d)	l = g_list_append( l, d )
#define G_LIST_INSERT_BEFORE(l,bd,d)	l = g_list_insert_before( l, bd, d )
#define G_LIST_REMOVE(l,d)	l = g_list_remove( l, d )
#define G_LIST_SORT(l,f)	l = g_list_sort( l, (GCompareFunc)f )
#define NEW(type)		(type *)xmalloc( sizeof(type) )
#define NEW_ARRAY(type,n)	(type *)xmalloc( (n) * sizeof(type) )
#define RESIZE(block,n,type)	block = (type *)xrealloc( block, (n) * sizeof(type) )
#define STRRECAT(str,addstr)	str = strrecat( str, addstr )

/* For when a switch should never default */
#define SWITCH_FAIL		default: g_assert_not_reached( ); exit( EXIT_FAILURE );

/* Cylindrical-space distance */
#define	_RTZDIST1(v1,v2)	SQR(v1.r) + SQR(v2.r) + SQR(v2.z - v1.z)
#define	_RTZDIST2(v1,v2)	(2.0 * v1.r * v2.r)
#define _RTZDIST3(v1,v2)	sin( RAD(v1.theta) ) * sin( RAD(v2.theta) )
#define _RTZDIST4(v1,v2)	cos( RAD(v1.theta) ) * cos( RAD(v2.theta) )
#define RTZ_DIST(v1,v2)		sqrt( fabs( _RTZDIST1(v1,v2) - _RTZDIST2(v1,v2) * (_RTZDIST3(v1,v2) + _RTZDIST4(v1,v2)) ) )

/* Returns information about the given node */
#define NODE_DESC(node)		((NodeDesc *)(node)->data)
#define DIR_NODE_DESC(dnode)	((DirNodeDesc *)(dnode)->data)
#define NODE_IS_DIR(node)	(NODE_DESC(node)->type == NODE_DIRECTORY)
#define NODE_IS_METANODE(node)	(NODE_DESC(node)->type == NODE_METANODE)
#define DIR_COLLAPSED(dnode)	(DIR_NODE_DESC(dnode)->deployment < EPSILON)
#define DIR_EXPANDED(dnode)	(DIR_NODE_DESC(dnode)->deployment > (1.0 - EPSILON))


/* Nonstandard but nice */
typedef gint64 int64;
typedef unsigned int bitfield;
typedef guint8 byte;
typedef gboolean boolean;

/* Program modes */
typedef enum {
	FSV_DISCV,
	FSV_MAPV,
	FSV_TREEV,
	FSV_SPLASH,
	FSV_NONE
} FsvMode;

/* The various types of nodes */
typedef enum {
	NODE_METANODE,
	NODE_DIRECTORY,
	NODE_REGFILE,
	NODE_SYMLINK,
	NODE_FIFO,
	NODE_SOCKET,
	NODE_CHARDEV,
	NODE_BLOCKDEV,
	NODE_UNKNOWN,
	NUM_NODE_TYPES
} NodeType;


/**** Global data structures ****************/

/* RGB color definition (components are in range [0, 1]) */
typedef struct _RGBcolor RGBcolor;
struct _RGBcolor {
	float	r;
	float	g;
	float	b;
};

/* Point/vector definition (2D Cartesian coordinates) */
typedef struct _XYvec XYvec;
struct _XYvec {
	double	x;
	double	y;
};

/* Point/vector definition (3D Cartesian coordinates) */
typedef struct _XYZvec XYZvec;
struct _XYZvec {
	double	x;
	double	y;
	double	z;
};

/* Point/vector definition (2D polar coordinates) */
typedef struct _RTvec RTvec;
struct _RTvec {
	double	r;
	double	theta;
};

/* Point/vector definition (3D cylindrical coordinates) */
typedef struct _RTZvec RTZvec;
struct _RTZvec {
	double	r;
	double	theta;
	double	z;
};

/* Base node descriptor. Describes a filesystem node
 * (file/symlink/whatever) */
typedef struct _NodeDesc NodeDesc;
struct _NodeDesc {
	NodeType	type;		/* Type of node */
	unsigned int	id;		/* Unique ID number */
	const char	*name;		/* Base name (w/o directory) */
	int64		size;		/* Size (bytes) */
	int64		size_alloc;	/* Size allocation on storage medium */
	uid_t		user_id;	/* Owner UID */
	gid_t		group_id;	/* Group GID */
	bitfield	perms : 10;	/* Permission flags */
	bitfield	flags : 2;	/* Extra (mode-specific) flags */
	time_t		atime;		/* Last access time */
	time_t		mtime;		/* Last modification time */
	time_t		ctime;		/* Last attribute change time */
	const RGBcolor	*color;		/* Node color */
	double		geomparams[5];	/* Geometry parameters */
};

/* Directories have their own extended descriptor */
typedef struct _DirNodeDesc DirNodeDesc;
struct _DirNodeDesc {
	NodeDesc	node_desc;
	double		geomparams2[3];	/* More geometry parameters */
	double		deployment;	/* 0 == collapsed, 1 == expanded */
	/* Subtree information. The quantities here do not include the
	 * contribution of the root of the subtree (i.e. THIS node) */
	struct {
		int64		size;	/* Total subtree size (bytes) */
		unsigned int	counts[NUM_NODE_TYPES]; /* Node type totals */
	} subtree;
	/* Following pointer should be of type GtkCTreeNode */
	void		*ctnode;	/* Directory tree entry */
	unsigned int	a_dlist;	/* Display list A */
	unsigned int	b_dlist;	/* Display list B */
	unsigned int	c_dlist;	/* Display list C */
	/* Flag: TRUE if directory geometry is being drawn expanded */
	bitfield	geom_expanded : 1;
	/* Flags: TRUE if geometry in X_dlist needs to be rebuilt */
	bitfield	a_dlist_stale : 1;
	bitfield	b_dlist_stale : 1;
	bitfield	c_dlist_stale : 1;
};

/* Generalized node descriptor */
union AnyNodeDesc {
	NodeDesc	node_desc;
	DirNodeDesc	dir_node_desc;
};

/* Node information struct. Everything here is a string.
 * get_node_info( ) fills in all the fields */
struct NodeInfo {
	char *name;		/* Name (without directory components) */
	char *prefix;		/* Leading directory components */
	char *size;		/* Size (in bytes) */
	char *size_abbr;	/* Abbreviated size (e.g. "5kB") */
	char *size_alloc;	/* Allocation size (bytes) */
	char *size_alloc_abbr;	/* Abbreviated allocation size */
	char *user_name;	/* Owner's user name */
	char *group_name;	/* Owner's group name */
	char *atime;		/* Last access time */
	char *mtime;		/* Last modification time */
	char *ctime;		/* Last attribute change time */
	/* For directories */
	char *subtree_size;	/* Total size of subtree (bytes) */
	char *subtree_size_abbr; /* Abbreviated total size of subtree */
	/* For regular files */
	char *file_type_desc;	/* Verbose description of file type */
	/* For symbolic links */
	char *target;		/* Target of symlink */
	char *abstarget;	/* Absolute name of target */
};

/* Global variables container */
struct Globals {
	/* Current program mode */
	FsvMode fsv_mode;

	/* The filesystem tree */
	GNode *fstree;

	/* Current node of interest */
	GNode *current_node;

	/* History of previously visited nodes
	 * (elements are of type GNode) */
	GList *history;

	/* TRUE when viewport needs to be redrawn */
	boolean need_redraw;
};


/**** Global variables ****************/

extern struct Globals globals;
extern char **node_type_xpms[NUM_NODE_TYPES];
extern char **node_type_mini_xpms[NUM_NODE_TYPES];
extern const char *node_type_names[NUM_NODE_TYPES];
extern const char *node_type_plural_names[NUM_NODE_TYPES];


/**** Prototypes for common library functions ****************/

#ifndef DEBUG
void *xmalloc( size_t size );
void *xrealloc( void *block, size_t size );
char *xstrdup( const char *string );
char *xstrredup( char *old_string, const char *string );
void xfree( void *block );
#endif /* not DEBUG */
char *strrecat( char *string, const char *add_string );
char *xstrstrip( char *string );
boolean xfork( void);
const char *xgetcwd( void);
double xgettime( void);
const char *i64toa( int64 number );
const char *abbrev_size( int64 size );
const char *node_absname( GNode *node );
GNode *node_named( const char *absname );
const struct NodeInfo *get_node_info( GNode *node );
const char *rgb2hex( RGBcolor *color );
RGBcolor hex2rgb( const char *hex_color );
RGBcolor rainbow_color( double x );
RGBcolor heat_color( double x );
GList *g_list_replace( GList *list, gpointer old_data, gpointer new_data );
GList *g_list_insert_before( GList *list, gpointer before_data, gpointer data );
int gnome_config_get_token( const char *path, const char **tokens );
void gnome_config_set_token( const char *path, int new_value, const char **tokens );
void quit( char *message );


/* end common.h */
