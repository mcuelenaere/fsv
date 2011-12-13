/* common.c */

/* Common library */

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

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/time.h>

#include "gui.h" /* gui_update( ) */

/* Node type icon XPM files */
#include "xmaps/folder.xpm"
#include "xmaps/regfile.xpm"
#include "xmaps/symlink.xpm"
#include "xmaps/fifo.xpm"
#include "xmaps/socket.xpm"
#include "xmaps/chardev.xpm"
#include "xmaps/blockdev.xpm"
#include "xmaps/unknown.xpm"

/* Mini node type icon XPM files */
#include "xmaps/mini-folder.xpm"
#include "xmaps/mini-regfile.xpm"
#include "xmaps/mini-symlink.xpm"
#include "xmaps/mini-fifo.xpm"
#include "xmaps/mini-socket.xpm"
#include "xmaps/mini-chardev.xpm"
#include "xmaps/mini-blockdev.xpm"
#include "xmaps/mini-unknown.xpm"


/* The global variables live here */
struct Globals globals;

/* Node type icon XPM table */
char **node_type_xpms[NUM_NODE_TYPES] = {
	NULL,
	folder_xpm,
	regfile_xpm,
	symlink_xpm,
	fifo_xpm,
	socket_xpm,
	chardev_xpm,
	blockdev_xpm,
	unknown_xpm
};

/* Mini node type icon XPM table */
char **node_type_mini_xpms[NUM_NODE_TYPES] = {
	NULL,
	mini_folder_xpm,
	mini_regfile_xpm,
	mini_symlink_xpm,
	mini_fifo_xpm,
	mini_socket_xpm,
	mini_chardev_xpm,
	mini_blockdev_xpm,
	mini_unknown_xpm
};

/* Full node type names */
const char *node_type_names[NUM_NODE_TYPES] = {
	NULL, /* "Metanode" */
	__("Directory"),
	__("Regular file"),
	__("Symbolic link"),
	__("Named pipe (FIFO)"),
	__("Network socket"),
	__("Character device"),
	__("Block device"),
	__("Unknown")
};

/* Plural node type names */
const char *node_type_plural_names[NUM_NODE_TYPES] = {
	NULL, /* "Metanodes" */
	__("Directories"),
	__("Regular files"),
	__("Symlinks"),
	__("Named pipes"),
	__("Sockets"),
	__("Char. devs."),
	__("Block devs."),
	__("Unknown")
};


/* Alternate versions of the following functions are used in debugging */
#ifndef DEBUG

/* malloc( ) wrapper function */
void *
xmalloc( size_t size )
{
	void *block = malloc( size );

	if (block == NULL)
		quit( _("Out of memory") );

	return block;
}


/* realloc( ) wrapper function */
void *
xrealloc( void *block, size_t size )
{
	void *new_block = realloc( block, size );

	if (new_block == NULL)
		quit( _("Out of memory") );

	return new_block;
}


/* strdup( ) wrapper function */
char *
xstrdup( const char *string )
{
	char *new_string = strdup( string );

	if (new_string == NULL)
		quit( _("Out of memory") );

	return new_string;
}


/* Love child of realloc( ) and strdup( ) */
char *
xstrredup( char *old_string, const char *string )
{
	char *new_string = xrealloc( old_string, strlen( string ) + 1 );

	if (new_string == NULL)
		quit( _("Out of memory") );
	strcpy( new_string, string );

        return new_string;
}


/* free( ) wrapper function */
void
xfree( void *block )
{
	free( block );
}

#endif /* not DEBUG */


/* Similar to strcpy( ), but with behavior that isn't undefined when
 * the two given strings overlap */
static char *
strmove( char *to, const char *from )
{
	memmove( to, from, strlen( from ) + 1 );
        return to;
}


/* Hybrid of strcat( ) and realloc( ). This appends add_string to string,
 * resizing string's memory allocation as necessary */
char *
strrecat( char *string, const char *add_string )
{
	int len;

	len = strlen( string ) + strlen( add_string ) + 1;
	RESIZE(string, len, char);
	strcat( string, add_string );

	return string;
}


/* Strips off any leading and trailing whitespace from a string, and trims
 * its memory allocation down to the bare minimum */
char *
xstrstrip( char *string )
{
        int i;
	char *string2;

	/* Remove leading whitespace */
	i = strspn( string, " \t" );
	if (i > 0)
		strmove( string, &string[i] );

	/* Remove trailing whitespace */
	for (i = strlen( string ) - 1; i >= -1; --i) {
		switch (string[MAX(0, i)]) {
			case ' ':
			case '\t':
                        /* Whitespace */
			continue;

			default:
			/* Non-whitespace character */
			break;
		}
                break;
	}
	string[i + 1] = '\0';

	/* Minimize memory allocation */
	string2 = xrealloc( string, (strlen( string ) + 1) * sizeof(char) );

	return string2;
}


/* fork() wrapper function. Returns TRUE if in newly created subprocess */
boolean
xfork( void )
{
	pid_t pid;

	pid = fork( );
	if (pid < 0)
		quit( "cannot fork( )" );

	return (pid == 0);
}


/* getcwd( ) wrapper function */
const char *
xgetcwd( void )
{
        static char *cwd = NULL;
	int len = 256;
	char *p = NULL;

	while (p == NULL) {
		RESIZE(cwd, len, char);
		p = getcwd( cwd, len );
		len *= 2;
	}

	cwd = xstrstrip( cwd );

	return cwd;
}


/* gettimeofday( ) wrapper function */
double
xgettime( void )
{
	struct timeval tv;

	gettimeofday( &tv, NULL );

	return (double)tv.tv_sec + 1.0e-6 * (double)tv.tv_usec;
}


/* This converts a 64-bit integer into a grouped number string
 * (e.g. 1000000 --> 1,000,000) */
const char *
i64toa( int64 number )
{
	static char strbuf1[256];
	int len, digit_count = 0;
	int n = 256, i;
	char strbuf0[256];
	char d;

	sprintf( strbuf0, "%lld", number );
	len = strlen( strbuf0 );
	for (i = len - 1; i >= 0; i--) {
		d = strbuf0[i];
		if ((digit_count % 3) == 0)
			strbuf1[--n] = ',';
		strbuf1[--n] = d;
		++digit_count;
	}
	strbuf1[255] = '\0';

	return &strbuf1[n];
}


/* Converts a byte quantity into "human-readable" abbreviated format
 * (e.g. 7632 --> 7.5kB, 1264245 --> 1.2MB, 1735892654 --> 1.6TB) */
const char *
abbrev_size( int64 size )
{
	static const char *suffixes[] = {
		__("B"),  /* Bytes */
		__("kB"), /* Kilobytes */
		__("MB"), /* Megabytes */
		__("GB"), /* Gigabytes */
		__("TB"), /* Terabytes */
		__("PB"), /* Petabytes */
		__("EB")  /* Exabytes (!) */
	};
	static char strbuf[64];
	double s;
	int m = 0;

	s = (double)size;
	while (s >= 1024.0) {
		++m;
		s /= 1024.0;
	}
	if ((m > 0) && (s < 100.0))
		sprintf( strbuf, "%.1f %s", s, _(suffixes[m]) );
	else
		sprintf( strbuf, "%.0f %s", s, _(suffixes[m]) );

	return strbuf;
}


/* Returns the absolute name of a node
 * (i.e. with all leading directory components) */
const char *
node_absname( GNode *node )
{
	static char *absname = NULL;
	GNode *up_node;
	int len, absname_len = 0;
	int i;
	const char *name;

	/* Determine length of absolute name */
	up_node = node;
	while (up_node != NULL) {
		name = NODE_DESC(up_node)->name;
		len = strlen( name );
		absname_len += len + 1;
		up_node = up_node->parent;
	}

	if (absname != NULL)
		xfree( absname );
	absname = NEW_ARRAY(char, absname_len);

	/* Build up absolute name */
	i = absname_len;
	up_node = node;
	while (up_node != NULL) {
		name = NODE_DESC(up_node)->name;
		len = strlen( name );
		absname[--i] = '/';
		i -= len;
		strncpy( &absname[i], name, len );
		up_node = up_node->parent;
	}
	absname[absname_len - 1] = '\0';

	if (!strncmp( absname, "//", 2 )) {
		/* Special cases when root directory is "/" */
		if (absname[2] == '/')
			return &absname[2]; /* avoid e.g. "///usr/blah" */
                else
			return &absname[1]; /* avoid "//" */
	}

	return absname;
}


/* This does roughly the opposite of node_absname( ): given an (absolute)
 * filename, return the corresponding node if it is present in the current
 * filesystem tree (NULL otherwise) */
GNode *
node_named( const char *absname )
{
	GNode *node;
	int len;
        const char *root_name;
	const char delimiters[] = "/";
	char *absname_partial_copy;
	char *name;

	/* The root directory name should be an initial substring of the
	 * desired node's absolute name. Otherwise, the desired node
	 * doesn't exist */
	root_name = node_absname( root_dnode );
	len = strlen( root_name );
	if (!strncmp( root_name, absname, len )) {
		if (len == 1) {
			/* An exception for when root_name is "/"
			 * (absname_partial_copy should begin either with
			 * a slash or a terminating null) */
			len = 0;
		}
                /* Copy the rest of the string into working space */
		absname_partial_copy = xstrdup( &absname[len] );
	}
	else
		return NULL;

	switch (strlen( absname_partial_copy )) {
		case 0:
		/* Absolute names of root directory and desired node
		 * match perfectly, but are not equal to "/" */
		g_assert( !strcmp( absname, node_absname( root_dnode ) ) );
		g_assert( strcmp( "/", absname ) );
		xfree( absname_partial_copy );
		return root_dnode;

		case 1:
		/* Absolute names of root directory and desired node
		 * match perfectly, both equal to "/" */
		g_assert( !strcmp( absname, node_absname( root_dnode ) ) );
		g_assert( !strcmp( "/", absname ) );
		xfree( absname_partial_copy );
		return root_dnode;

		default:
		/* Root directory is not the desired node, so we'll have
		 * to perform component-by-component matching */
		break;
	}

	name = strtok( absname_partial_copy, delimiters );
	node = root_dnode->children;
	while (node != NULL) {
		if (!strcmp( name, NODE_DESC(node)->name )) {
			name = strtok( NULL, delimiters );
			if (name == NULL)
                                break;
			node = node->children;
			continue;
		}
		node = node->next;
	}

#ifdef DEBUG
	if (node != NULL)
		g_assert( !strcmp( absname, node_absname( node ) ) );
#endif

	xfree( absname_partial_copy );

	return node;
}


#ifdef HAVE_FILE_COMMAND
/* Runs the 'file' command on the given file, and returns the output
 * (a verbose description of the file type) */
static char *
get_file_type_desc( const char *filename )
{
	static char *cmd_output = NULL;
	FILE *cmd;
	double t0;
	int len, c, i = 0;
	char *cmd_line;

	/* Allocate output buffer */
	RESIZE(cmd_output, strlen( filename ) + 1024, char);

	/* Construct command line */
	len = strlen( FILE_COMMAND ) + strlen( filename ) - 1;
	cmd_line = NEW_ARRAY(char, len);
	sprintf( cmd_line, FILE_COMMAND, filename );

	/* Open command stream */
	cmd = popen( cmd_line, "r" );
	xfree( cmd_line );
	if (cmd == NULL) {
		strcpy( cmd_output, _("Could not execute 'file' command") );
		return cmd_output;
	}

	/* Read loop */
	t0 = xgettime( );
	while (!feof( cmd )) {
		/* Read command's stdout */
		c = fgetc( cmd );
		if (c != EOF)
			cmd_output[i++] = c;

		/* Check for timeout condition */
		if ((xgettime( ) - t0) > 5.0) {
			fclose( cmd ); /* Is this allowed? */
			strcpy( cmd_output, _("('file' command timed out)") );
			return cmd_output;
		}

		/* Keep the GUI responsive */
		gui_update( );
	}
	pclose( cmd );
	cmd_output[i] = '\0';

	len = strlen( filename );
	if (!strncmp( filename, cmd_output, len )) {
		/* Remove prepended "filename: " from output */
		return &cmd_output[len + 2];
	}

	return cmd_output;
}
#endif /* HAVE_FILE_COMMAND */


/* Returns the target of a symbolic link */
static char *
read_symlink( const char *linkname )
{
	static char *target = NULL;
	int len = 256, n;

	for (;;) {
		RESIZE(target, len, char);
		n = readlink( linkname, target, len );
		if (n < len)
			break;
		len *= 2;
	}
	target[n] = '\0';
	RESIZE(target, n + 1, char);

	return target;
}


/* Helper function for absname_merge( ). This takes an absolute name, and
 * the position of a slash or null terminator in the string. It removes the
 * directory component preceding that position, and returns the position of
 * the slash preceding the next component in the name. Examples:
 *     int i;
 *     char absname[16];
 *     strcpy( absname, "/abc/def/ghi" );
 *     i = remove_component( absname, 8 );  // i => 4, absname => "/abc/ghi"
 *     strcpy( absname, "/abc/def/ghi" );
 *     i = remove_component( absname, 12 );  // i => 8, absname => "/abc/def"
 */
static int
remove_component( char *absname, int pos )
{
	int i;

	g_assert( (absname[pos] == '/') || (absname[pos] == '\0') );

	if (pos == 0) {
		/* Warning: attempting to remove a component higher than
		 * the root directory! Silently ignore this foolishness */
                return 0;
	}

	for (i = pos - 1; i >= 0; --i) {
		if (absname[i] == '/') {
                        /* Collapse string */
			strmove( &absname[i], &absname[pos] );
			break;
		}
	}

        return i;
}


/* This takes two names: an absolute directory name, and a name relative
 * to the directory, and it runs them together into one absolute name.
 * This can be (and is) used to find symlink targets. Some examples:
 *     absname_merge( "/usr/bin", "gcc" ) => "/usr/bin/gcc"
 *     absname_merge( "/usr/local/lib", "../../lib/libblah.so" ) => "/usr/lib/libblah.so"
 *     absname_merge( "/home/fox", "./../skunk/./././weird" ) => "/home/skunk/weird"
 *     absname_merge( "/home/skunk", "../fox////./cool" ) => "/home/fox/cool"
 */
static char *
absname_merge( const char *dirname, const char *rel_name )
{
	static char *absname = NULL;
	int i, len, dot_count = 0;
        boolean prev_char_is_slash = FALSE;

	len = strlen( dirname ) + strlen( rel_name ) + 2;
	RESIZE(absname, len, char);

	if (strlen( rel_name ) == 0) {
		/* Relative part is empty. This is weird. */
		strcpy( absname, dirname );
	}
	else if (rel_name[0] == '/') {
		/* Relative part is an absolute name in itself, thus the
                 * directory name is completely irrelevant */
		strcpy( absname, rel_name );
	}
	else {
		/* Concatenate the two names together */
		strcpy( absname, dirname );
		strcat( absname, "/" );
		strcat( absname, rel_name );
	}

	/* Get rid of all ".." components, extra slashes, etc. */
	for (i = 0; ; ++i) {
		switch (absname[i]) {
			case '.':
			prev_char_is_slash = FALSE;
			++dot_count;
			break;

			case '/':
			/* End of component reached */
			if (prev_char_is_slash) {
                                /* Remove superfluous slash */
				strmove( &absname[i], &absname[i + 1] );
				--i;
				break;
			}
			else
				prev_char_is_slash = TRUE;
			/* !break */

			case '\0':
			/* End of absolute name reached */
			switch (dot_count) {
				case 1:
				/* Remove superfluous "." component */
				i = remove_component( absname, i );
				break;

				case 2:
				/* ".." component cancels out itself and
				 * the previous component */
				i = remove_component( absname, i );
				i = remove_component( absname, i );
				break;

				case 0:
				default:
				/* Nothing to do at this point */
                                break;
			}
                        dot_count = 0;
			break;

			default:
			/* Normal character */
			prev_char_is_slash = FALSE;
			dot_count = 0;
			break;
		}

		if (absname[i] == '\0')
			break;
	}

	absname = xstrstrip( absname );

	return absname;
}


/* Returns a NodeInfo struct for the given node */
const struct NodeInfo *
get_node_info( GNode *node )
{
	static struct NodeInfo ninfo = {
		NULL,	/* name */
		NULL,	/* prefix */
		NULL,	/* size */
		NULL,	/* size_abbr */
		NULL,	/* size_alloc */
		NULL,	/* size_alloc_abbr */
		NULL,	/* user_name */
		NULL,	/* group_name */
		NULL,	/* atime */
		NULL,	/* mtime */
		NULL,	/* ctime */
		NULL,	/* subtree_size */
		NULL,	/* subtree_size_abbr */
		NULL,	/* file_type_desc */
		NULL,	/* target */
		NULL	/* abstarget */
	};
	struct passwd *pw;
	struct group *gr;
	static char blank[] = "-";
	const char *absname;
	const char *cstr;
	char *str;

	absname = node_absname( node );

	/* Name */
	if (strlen( NODE_DESC(node)->name ) > 0)
		cstr = NODE_DESC(node)->name;
	else
		cstr = _("/. (root)");
	ninfo.name = xstrredup( ninfo.name, cstr );
	/* Prefix */
	str = g_dirname( absname );
	if (!strcmp( str, "/" )) {
		g_free( str );
		str = g_strdup( _("/. (root)") );
	}
	ninfo.prefix = xstrredup( ninfo.prefix, str );
	g_free( str );

	/* Size */
	ninfo.size = xstrredup( ninfo.size, i64toa( NODE_DESC(node)->size ) );
	ninfo.size_abbr = xstrredup( ninfo.size_abbr, abbrev_size( NODE_DESC(node)->size ) );
	/* Allocation size */
	ninfo.size_alloc = xstrredup( ninfo.size_alloc, i64toa( NODE_DESC(node)->size_alloc ) );
	ninfo.size_alloc_abbr = xstrredup( ninfo.size_alloc_abbr, abbrev_size( NODE_DESC(node)->size_alloc ) );

	/* User name */
	pw = getpwuid( NODE_DESC(node)->user_id );
	if (pw == NULL)
		cstr = _("Unknown");
	else
		cstr = pw->pw_name;
	ninfo.user_name = xstrredup( ninfo.user_name, cstr );
	/* Group name */
	gr = getgrgid( NODE_DESC(node)->group_id );
	if (gr == NULL)
		cstr = _("Unknown");
	else
		cstr = gr->gr_name;
	ninfo.group_name = xstrredup( ninfo.group_name, cstr );

	/* Timestamps - remember to strip ctime's trailing newlines */
	ninfo.atime = xstrredup( ninfo.atime, ctime( &NODE_DESC(node)->atime ) );
        ninfo.atime[strlen( ninfo.atime ) - 1] = '\0';
	ninfo.mtime = xstrredup( ninfo.mtime, ctime( &NODE_DESC(node)->mtime ) );
        ninfo.mtime[strlen( ninfo.mtime ) - 1] = '\0';
	ninfo.ctime = xstrredup( ninfo.ctime, ctime( &NODE_DESC(node)->ctime ) );
	ninfo.ctime[strlen( ninfo.ctime ) - 1] = '\0';

	/* For directories: subtree size */
	if (NODE_DESC(node)->type == NODE_DIRECTORY) {
		ninfo.subtree_size = xstrredup( ninfo.subtree_size, i64toa( DIR_NODE_DESC(node)->subtree.size ) );
		ninfo.subtree_size_abbr = xstrredup( ninfo.subtree_size_abbr, abbrev_size( DIR_NODE_DESC(node)->subtree.size ) );
	}
	else {
		ninfo.subtree_size = xstrredup( ninfo.subtree_size, blank );
		ninfo.subtree_size_abbr = xstrredup( ninfo.subtree_size_abbr, blank );
	}

	/* For regular files: file type description */
	if (NODE_DESC(node)->type == NODE_REGFILE)
		ninfo.file_type_desc = get_file_type_desc( absname );
	else
		ninfo.file_type_desc = blank;

	/* For symbolic links: target name(s) */
	if (NODE_DESC(node)->type == NODE_SYMLINK) {
		ninfo.target = read_symlink( absname );
		str = g_dirname( absname );
		ninfo.abstarget = absname_merge( str, ninfo.target );
		g_free( str );
	}
	else {
		ninfo.target = blank;
		ninfo.abstarget = blank;
	}

	return &ninfo;
}


/* Generates a hexadecimal color triplet */
const char *
rgb2hex( RGBcolor *color )
{
	static char hexbuf[8];
	int r, g, b;

	r = (int)floor( 255.0 * color->r + 0.5 );
	g = (int)floor( 255.0 * color->g + 0.5 );
	b = (int)floor( 255.0 * color->b + 0.5 );

	sprintf( hexbuf, "#%02X%02X%02X", r, g, b );

	return hexbuf;
}


/* Parses a hexadecimal color triplet */
RGBcolor
hex2rgb( const char *hex_color )
{
	RGBcolor rgb_color;
	int c[3] = { 0, 0, 0 };
	int x, y, i;
	const char *hex;

	hex = hex_color;
	if (hex[0] == '#')
		++hex;
	for (i = 0; i < 6; i++) {
		x = hex[i];
		if (x == '\0')
			break;
		else if ((x >= '0') && (x <= '9'))
			y = x - '0';
		else if ((x >= 'A') && (x <= 'F'))
			y = 10 + x - 'A';
		else if ((x >= 'a') && (x <= 'f'))
			y = 10 + x - 'a';
		else
			y = 8;

		if (!(i & 1))
			y <<= 4;
		c[i >> 1] += y;
	}

	rgb_color.r = (float)c[0] / 255.0;
	rgb_color.g = (float)c[1] / 255.0;
	rgb_color.b = (float)c[2] / 255.0;

	return rgb_color;
}


/* Returns a color at the given fractional position in a rainbow spectrum
 * (0 == red, 1 == magenta, 0.5 == around green, etc.) */
RGBcolor
rainbow_color( double x )
{
	RGBcolor color;
	double h, q, t;

	g_assert( (x >= 0.0) && (x <= 1.0) );

	h = MIN(4.9999, 5.0 * x);
	q = 0.5 * (1.0 + cos( PI * (h - floor( h ))));
        t = 1.0 - q;

	switch ((int)floor( h )) {
		case 0:
		color.r = 1.0;
		color.g = t;
		color.b = 0.0;
		break;

		case 1:
		color.r = q;
		color.g = 1.0;
		color.b = 0.0;
		break;

		case 2:
		color.r = 0.0;
		color.g = 1.0;
		color.b = t;
		break;

		case 3:
		color.r = 0.0;
		color.g = q;
		color.b = 1.0;
		break;

		case 4:
		color.r = t;
		color.g = 0.0;
		color.b = 1.0;
		break;

		SWITCH_FAIL
	}

        return color;
}


/* Returns a color at the given fractional position in a heat spectrum
 * (0 == cold, 1 == hot) */
RGBcolor
heat_color( double x )
{
	RGBcolor color;

	g_assert( (x >= 0.0) && (x <= 1.0) );

	color.r = 0.5 * (1.0 - cos( PI * CLAMP(2.0 * x, 0.0, 1.0) ));
	color.g = 0.5 * (1.0 - cos( PI * CLAMP(2.0 * x - 0.5, 0.0, 1.0) ));
	color.b = 0.5 * (1.0 - cos( PI * CLAMP(2.0 * x - 1.0, 0.0, 1.0) ));

	return color;
}


/* GLib-style routine. This replaces one element in a GList with another
 * (and does nothing if the former isn't in the list) */
GList *
g_list_replace( GList *list, gpointer old_data, gpointer new_data )
{
	GList *llink;

	llink = g_list_find( list, old_data );

	if (llink != NULL)
		llink->data = new_data;

	return llink;
}


/* GLib-style routine. Inserts a new element "data" into a list, before the
 * element "before_data". If "before_data" is NULL, then the new element is
 * added to the end of the list. Returns the updated list */
GList *
g_list_insert_before( GList *list, gpointer before_data, gpointer data )
{
	GList *before_llink;
	GList *new_llink;
	GList *updated_list;

	g_return_val_if_fail( list != NULL, NULL );

	if (before_data == NULL)
		updated_list = g_list_append( list, data );
	else {
		before_llink = g_list_find( list, before_data );
		g_return_val_if_fail( before_llink != NULL, list );
		new_llink = g_list_prepend( before_llink, data );
		if (before_llink == list)
			updated_list = new_llink;
		else
			updated_list = list;
	}

	return updated_list;
}


/* The wrong way out */
void
quit( char *message )
{
	fprintf( stderr, "ERROR: %s\n", message );
	fflush( stderr );

	exit( EXIT_FAILURE );
}


/* end common.c */
