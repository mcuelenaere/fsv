/* debug.c */

/* Debugging library */

/* Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,   
 * distribute, sublicense, and/or sell copies of the Software, and to   
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#define G_LOG_DOMAIN "debug"


#include "config.h"
#include <stddef.h>
#include <glib.h>
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>


/* Summary report shows these many of the most memory-hungry lines of
 * source code */
#define SUMMARY_TOP_N_LINES	10

/* Pass this to fecundity_report( ) to see all source code line stats */
#define REPORT_ALL_LINES	1000000000

/* Assume no string should be longer than this */
#define MAX_STRING_LENGTH	16384

/* We want to be able to use these */
#undef g_list_alloc
#undef g_list_prepend
#undef g_list_append
#undef g_list_insert
#undef g_list_insert_sorted
#undef g_list_remove
#undef g_list_copy
#undef g_list_free_1
#undef g_list_free
#undef g_node_new
#undef g_node_destroy

/* Types of memory blocks */
#define BLOCK_DATA	"data"
#define BLOCK_STRING	"string"
#define BLOCK_GLIST	"GList"
#define BLOCK_GNODE	"GNode"


typedef unsigned char byte;
typedef gboolean boolean;

/* Dossier for a memory block */
struct MemBlockInfo {
	void *block;
	size_t size;
	const char *type;
	const char *src_file;
	int src_line;
	time_t alloc_time;
	int resize_count;
};

/* For a line of source code */
struct SourceInfo {
	const char *src_file;
	int src_line;
	const char *block_type;
	int block_count;
	size_t byte_count;
};

/* The memory block database. The elements of this list are of type
 * 'struct MemBlockInfo' */
static GList *mem_block_info_list = NULL;

/* Running totals */
static int total_blocks = 0;
static size_t total_bytes = 0;


void
debug_init( void )
{
	static const char fname[] = "debug_init";
	GAllocator *debug_allocator;

	/* "Hi, I'm not a production executable!" */
	g_message( "[%s] Debugging routines say hello", fname );

	/* Tolerate no weirdness in Gtkland */
	g_log_set_fatal_mask( "Gtk", G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL );

	/* Set up single-unit allocators for these GLib types */
	debug_allocator = g_allocator_new( "GSList debug allocator", 1 );
	g_slist_push_allocator( debug_allocator );
	debug_allocator = g_allocator_new( "GList debug allocator", 1 );
	g_list_push_allocator( debug_allocator );
	debug_allocator = g_allocator_new( "GNode debug allocator", 1 );
	g_node_push_allocator( debug_allocator );
}


/* Helper function for fecundity_report( ). Used to sort by originating
 * source code location */
static int
mbi_origin_compare( const struct MemBlockInfo *mbi1, const struct MemBlockInfo *mbi2 )
{
	int x;

	x = strcmp( mbi1->src_file, mbi2->src_file );
	if (x != 0)
		return x;

	return (mbi1->src_line - mbi2->src_line);
}


/* Helper function for fecundity_report( ). Used to sort by descending
 * order of source code line fecundity */
static int
source_fecundity_compare( const struct SourceInfo *srci1, const struct SourceInfo *srci2 )
{
	return (srci2->block_count - srci1->block_count);
}


/* Produces a report on the top n most fecund lines of source code
 * (i.e. those lines that have produced the largest number of currently
 * allocated blocks) */
static void
fecundity_report( int top_n_lines )
{
        struct MemBlockInfo *mbi, *prev_mbi = NULL;
	struct SourceInfo *srci = NULL;
	GList *mbi_llink;
	GList *srci_list = NULL, *srci_llink;
        int i;
	boolean new_source_line;

	/* Sort the memory database by source code line locations, so
	 * that the same lines of code are grouped together */
	mem_block_info_list = g_list_sort( mem_block_info_list, (GCompareFunc)mbi_origin_compare );

	/* Now, run through the list, and for each unique line of
	 * source code, create a SourceInfo record */

	/* Scan through database */
	mbi_llink = mem_block_info_list;
	while (mbi_llink != NULL) {
		mbi = (struct MemBlockInfo *)mbi_llink->data;

		/* Determine if this block was created by a different
		 * line of source than the previous one */
		if (prev_mbi == NULL)
			new_source_line = TRUE;
		else if (mbi_origin_compare( mbi, prev_mbi ))
			new_source_line = TRUE;
		else
			new_source_line = FALSE;

		if (new_source_line) {
			/* Create new source info record */
			srci = malloc( sizeof(struct SourceInfo) );
			srci->src_file = mbi->src_file;
			srci->src_line = mbi->src_line;
			srci->block_type = mbi->type;
			srci->block_count = 1;
			srci->byte_count = mbi->size;
			/* and add it to the list */
			srci_list = g_list_prepend( srci_list, srci );
		}
		else {
			/* Increment counts for current line of code */
			++srci->block_count;
			srci->byte_count += mbi->size;
		}

		prev_mbi = mbi;
		mbi_llink = mbi_llink->next;
	}

	/* Sort the source code locations by fecundity */
	srci_list = g_list_sort( srci_list, (GCompareFunc)source_fecundity_compare );

	/* Print out (up to) the top n most fruitful lines */
        if (top_n_lines < REPORT_ALL_LINES)
		g_message( "==== Top %d most fecund source lines ====", top_n_lines );
	else
		g_message( "===== Allocations by line fecundity =====" );
        srci_llink = srci_list;
	for (i = 1; (i <= top_n_lines) && (srci_llink != NULL); i++) {
		srci = (struct SourceInfo *)srci_llink->data;
		g_message( "%d.  %s:%d    \t%d %s block(s)  (%u bytes)", i, srci->src_file, srci->src_line, srci->block_count, srci->block_type, srci->byte_count );
		srci_llink = srci_llink->next;
	}
	g_message( "=========================================" );

	/* Clean up */
        srci_llink = srci_list;
	while (srci_llink != NULL) {
		srci = (struct SourceInfo *)srci_llink->data;
		free( srci );
		srci_llink = srci_llink->next;
	}
	g_list_free( srci_list );
}


/* Prints out total number of blocks and bytes currently allocated, plus
 * the deltas from the last time this was called */
void
debug_show_mem_totals( void )
{
	static int last_total_blocks = 0;
	static size_t last_total_bytes = 0;

	g_message( "Allocated: %d blocks, %u bytes (%+d, %+d)", total_blocks, total_bytes, total_blocks - last_total_blocks, (int)total_bytes - (int)last_total_bytes );
	last_total_blocks = total_blocks;
	last_total_bytes = total_bytes;
}


void
debug_show_mem_summary( void )
{
	fecundity_report( SUMMARY_TOP_N_LINES );
	debug_show_mem_totals( );
}


/* Prints out statistics on outstanding memory blocks */
void
debug_show_mem_stats( void )
{
	fecundity_report( REPORT_ALL_LINES );
	debug_show_mem_totals( );
}


/* Creates a new block record, and adds it to the database */
static void
new_block_info( void *block, size_t size, const char *type, const char *src_file, int src_line )
{
	struct MemBlockInfo *mbi;

        /* Create new block info record */
	mbi = malloc( sizeof(struct MemBlockInfo) );
	mbi->block = block;
	mbi->size = size;
        mbi->type = type;
	mbi->src_file = src_file;
	mbi->src_line = src_line;
	mbi->alloc_time = time( NULL );
	mbi->resize_count = 0;

	/* Add to database */
	mem_block_info_list = g_list_prepend( mem_block_info_list, mbi );

	/* Add to running totals */
	total_bytes += size;
	++total_blocks;
}


/* Adjusts the size of a given block of memory */
static void
update_block_info( struct MemBlockInfo *mbi, void *block, size_t size )
{
        /* Adjust running byte total */
	total_bytes += size;
	total_bytes -= mbi->size;

	/* Update block info */
	mbi->block = block;
	mbi->size = size;
	++mbi->resize_count;
}


/* Finds, and optionally removes a particular memory block info record
 * from the database. Returns NULL if no corresponding record is found
 * for the block */
static struct MemBlockInfo *
find_block_info( const void *block, boolean remove )
{
	struct MemBlockInfo *mbi;
	GList *mbi_llink;

	mbi_llink = mem_block_info_list;
	while (mbi_llink != NULL) {
		mbi = (struct MemBlockInfo *)mbi_llink->data;
		if (block == mbi->block) {
			if (remove) {
				mem_block_info_list = g_list_remove_link( mem_block_info_list, mbi_llink );
				g_list_free_1( mbi_llink );
			}
			return mbi;
		}
		mbi_llink = mbi_llink->next;
	}

	return NULL;
}


/* This finishes off a block. However, it is still up to the caller to
 * actually free whatever is at mbi->block (and that should be done
 * *after* this is called) */
static void
destroy_block_info( struct MemBlockInfo *mbi )
{
	int i;

	/* Zero out contents before freeing */
	for (i = 0; i < mbi->size; i++)
		((byte *)mbi->block)[i] = 0;

	/* Subtract from running totals */
	total_bytes -= mbi->size;
	--total_blocks;

	free( mbi );
}


/* Something to grab the attention */
static void
critical_message( const char *src_file, int src_line )
{
	fprintf( stderr, "CRITICAL>>> %s:%d\a\n", src_file, src_line );
	fprintf( stderr, "CRITICAL>>> " );
	fflush( stderr );
}


/* Determines if the given string is really a string, or just random
 * garbage in memoryland. Returns TRUE if string is good */
static boolean
check_string( const char *string, const char *src_file, int src_line, const char *fname )
{
	int i = 0;
	boolean all_ascii = TRUE;

	while (string[i] != '\0') {
		if (i > MAX_STRING_LENGTH) {
			critical_message( src_file, src_line );
			fprintf( stderr, "[%s] Source string is over %d characters long\n", fname, MAX_STRING_LENGTH );
			return FALSE;
		}
		if (all_ascii && !isascii( string[i] )) {
			g_warning( "%s:%d [%s] Source string has non-ASCII characters", src_file, src_line, fname );
			all_ascii = FALSE;
		}
		++i;
	}

	return all_ascii;
}


void *
debug_malloc( size_t size, const char *src_file, int src_line )
{
	static const char fname[] = "debug_malloc";
	void *block;
	int i;

	if (size == 0)
		g_warning( "%s:%d [%s] Allocating zero-size block", src_file, src_line, fname );

	/* Allocate new block */
	block = malloc( size );
	if (block == NULL)
		g_error( "%s:%d [%s] malloc( %u ) returned NULL", src_file, src_line, fname, size );

	/* Fill newly allocated block with random junk */
	for (i = 0; i < size; i++)
		((byte *)block)[i] = (byte)(rand( ) & 0xFF);

	new_block_info( block, size, BLOCK_DATA, src_file, src_line );

	return block;
}


void *
debug_realloc( void *block, size_t size, const char *src_file, int src_line )
{
        static const char fname[] = "debug_realloc";
	struct MemBlockInfo *mbi;
	void *new_block;

	if (block == NULL)
		return debug_malloc( size, src_file, src_line );

	mbi = find_block_info( block, FALSE );
	if (mbi == NULL)
		g_error( "%s:%d [%s] Attempted to resize unknown block", src_file, src_line, fname );

	if (size == 0)
		g_warning( "%s:%d [%s] Resizing block to zero size", src_file, src_line, fname );
	if (size == mbi->size)
		g_message( "%s:%d [%s] Resizing block to same size", src_file, src_line, fname );

	new_block = realloc( block, size );
	if (new_block == NULL)
		g_error( "%s:%d [%s] realloc( %p, %u ) returned NULL", src_file, src_line, fname, block, size );

	update_block_info( mbi, new_block, size );

	return new_block;
}


void *
debug_memdup( void *mem, size_t size, const char *src_file, int src_line )
{
	static const char fname[] = "debug_memdup";
	void *block;

	if (size == 0)
		g_warning( "%s:%d [%s] Duplicating zero-length region", src_file, src_line, fname );

	block = malloc( size );
	if (block == NULL)
		g_error( "%s:%d [%s] malloc( %u ) returned NULL", src_file, src_line, fname, size );
	memcpy( block, mem, size );

	new_block_info( block, size, BLOCK_DATA, src_file, src_line );

	return block;
}


char *
debug_strdup( const char *string, const char *src_file, int src_line )
{
        static const char fname[] = "debug_strdup";
	size_t size;
	char *new_string;

	if (string == NULL)
		g_error( "%s:%d [%s] Attempted to copy NULL string", src_file, src_line, fname );

	check_string( string, src_file, src_line, fname );

	size = strlen( string ) + 1;
	new_string = strdup( string );
	if (new_string == NULL)
		g_error( "%s:%d [%s] strdup( ) returned NULL", src_file, src_line, fname );

	new_block_info( new_string, size, BLOCK_STRING, src_file, src_line );

	return new_string;
}


char *
debug_strredup( char *old_string, const char *string, const char *src_file, int src_line )
{
	static const char fname[] = "debug_strredup";
	struct MemBlockInfo *mbi;
	size_t size;
	char *new_string;

	if (old_string == NULL)
		return debug_strdup( string, src_file, src_line );

	mbi = find_block_info( old_string, FALSE );
	if (mbi == NULL)
		g_error( "%s:%d [%s] Attempted to resize unknown string", src_file, src_line, fname );

	if (string == NULL)
		g_error( "%s:%d [%s] Attempted to copy NULL string", src_file, src_line, fname );

	check_string( string, src_file, src_line, fname );

        size = strlen( string ) + 1;
	new_string = realloc( old_string, size );
	if (new_string == NULL)
		g_error( "%s:%d [%s] realloc( %p, %u ) returned NULL", src_file, src_line, fname, old_string, size );
	strcpy( new_string, string );

	update_block_info( mbi, new_string, size );

        return new_string;
}


void
debug_free( void *block, const char *src_file, int src_line )
{
        static const char fname[] = "debug_free";
	struct MemBlockInfo *mbi;

	if (block == NULL) {
		critical_message( src_file, src_line );
		fprintf( stderr, "[%s] Attempted to free NULL block\n", fname );
		return;
	}

	mbi = find_block_info( block, TRUE );
	if (mbi == NULL) {
		critical_message( src_file, src_line );
		fprintf( stderr, "[%s] Attempted to free unknown block\n", fname );
		return;
	}

	destroy_block_info( mbi );

        free( block );
}


/* Special form of free( ) for use by functions outside our jurisdiction
 * (when passed as a function pointer, e.g. a destroy notify argument to
 * some library routine) */
void
debug_ufree( void *block )
{
	debug_free( block, "(function)", -1 );
}


GList *
debug_g_list_alloc( const char *src_file, int src_line )
{
	GList *llink;

	llink = g_list_alloc( );
	new_block_info( llink, sizeof(GList), BLOCK_GLIST, src_file, src_line );

	return llink;
}


GList *
debug_g_list_prepend( GList *list, gpointer data, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_prepend";
	GList *list2, *llink;

	if (g_list_find( list, data ) != NULL)
		g_warning( "%s:%d [%s] Prepending duplicate element", src_file, src_line, fname );

	list2 = g_list_prepend( list, data );

	llink = list2;
	new_block_info( llink, sizeof(GList), BLOCK_GLIST, src_file, src_line );

	return list2;
}


GList *
debug_g_list_append( GList *list, gpointer data, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_append";
	GList *list2, *llink;

	if (g_list_find( list, data ) != NULL)
		g_warning( "%s:%d [%s] Appending duplicate element", src_file, src_line, fname );

	list2 = g_list_append( list, data );

	llink = g_list_last( list2 );
	new_block_info( llink, sizeof(GList), BLOCK_GLIST, src_file, src_line );

	return list2;
}


GList *
debug_g_list_insert( GList *list, gpointer data, gint position, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_insert";
	GList *list2, *llink;

	if (g_list_find( list, data ) != NULL)
		g_warning( "%s:%d [%s] Inserting duplicate element", src_file, src_line, fname );

	list2 = g_list_insert( list, data, position );

	llink = g_list_nth( list2, position );
	g_assert( llink != NULL );
	new_block_info( llink, sizeof(GList), BLOCK_GLIST, src_file, src_line );

	return list2;
}


GList *
debug_g_list_insert_sorted( GList *list, gpointer data, GCompareFunc func, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_insert_sorted";
	GList *list2, *llink;

	if (g_list_find( list, data ) != NULL)
		g_warning( "%s:%d [%s] Inserting duplicate element", src_file, src_line, fname );

	list2 = g_list_insert_sorted( list, data, func );

	/* Because g_list_find( ) might return an already-known block
	 * (i.e. a duplicate), we need to be careful here */
	llink = list2;
	for (;;) {
		llink = g_list_find( llink, data );
		if (find_block_info( llink, FALSE ) == NULL)
			break; /* this is the new block */
		llink = llink->next;
	}
	new_block_info( llink, sizeof(GList), BLOCK_GLIST, src_file, src_line );

	return list2;
}


GList *
debug_g_list_remove( GList *list, gpointer data, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_remove";
	GList *list2, *llink;

	llink = g_list_find( list, data );
	if (llink == NULL) {
		critical_message( src_file, src_line );
		fprintf( stderr, "[%s] Attempted to remove unknown link\n", fname );
		return list;
	}

	list2 = g_list_remove_link( list, llink );
	debug_g_list_free_1( llink, src_file, src_line );

	return list2;
}


GList *
debug_g_list_copy( GList *list, const char *src_file, int src_line )
{

	/* Haven't had a need for this one yet */
	g_assert_not_reached( );

        return NULL;
}


void
debug_g_list_free_1( GList *llink, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_list_free_1";
	struct MemBlockInfo *mbi;

	mbi = find_block_info( llink, TRUE );
	if (mbi == NULL) {
		critical_message( src_file, src_line );
		fprintf( stderr, "[%s] Attempted to free unknown link\n", fname );
		return;
	}

	destroy_block_info( mbi );
	g_list_free_1( llink );
}


void
debug_g_list_free( GList *list, const char *src_file, int src_line )
{
	GList *llink, *next_llink;

	llink = list;
	while (llink != NULL) {
		next_llink = llink->next;
		debug_g_list_free_1( llink, src_file, src_line );
		llink = next_llink;
	}
}


GNode *
debug_g_node_new( gpointer data, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_node_new";
	GNode *node;

	if (data == NULL)
		g_message( "%s:%d [%s] Creating node with NULL data", src_file, src_line, fname );

	node = g_node_new( data );
	new_block_info( node, sizeof(GNode), BLOCK_GNODE, src_file, src_line );

	return node;
}


void
debug_g_node_destroy( GNode *node, const char *src_file, int src_line )
{
	static const char fname[] = "debug_g_node_destroy";
	struct MemBlockInfo *mbi;
	GNode *n, *n_next;

	n = node->children;
	while (n != NULL) {
                n_next = n->next;
		g_node_unlink( n );
		debug_g_node_destroy( n, src_file, src_line );
                n = n_next;
	}

	mbi = find_block_info( node, TRUE );
	if (mbi == NULL) {
		critical_message( src_file, src_line );
		fprintf( stderr, "[%s] Attempted to destroy unknown node\n", fname );
		return;
	}

	destroy_block_info( mbi );
	g_node_unlink( node );
	g_node_destroy( node );
}


/* end debug.c */
