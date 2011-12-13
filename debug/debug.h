/* debug.h */

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


#define xmalloc(size)		debug_malloc( size, __FILE__, __LINE__ )
#define xrealloc(block, size)	debug_realloc( block, size, __FILE__, __LINE__ )
#define xmemdup(mem, size)	debug_memdup( mem, size, __FILE__, __LINE__ )
#define xstrdup(string)		debug_strdup( string, __FILE__, __LINE__ )
#define xstrredup(old_str, str)	debug_strredup( old_str, str, __FILE__, __LINE__ )
#define xfree(block)		debug_free( block, __FILE__, __LINE__ )
#define _xfree			debug_ufree

#define g_list_alloc( )		debug_g_list_alloc( __FILE__, __LINE__ )
#define g_list_prepend(list, data)	debug_g_list_prepend( list, data, __FILE__, __LINE__ )
#define g_list_append(list, data)	debug_g_list_append( list, data, __FILE__, __LINE__ )
#define g_list_insert(list, data, pos)	debug_g_list_insert( list, data, pos, __FILE__, __LINE__ )
#define g_list_insert_sorted(list, data, func)	debug_g_list_insert_sorted( list, data, func, __FILE__, __LINE__ )
#define g_list_remove(list, data)	debug_g_list_remove( list, data, __FILE__, __LINE__ )
#define g_list_copy(list)	debug_g_list_copy( list, __FILE__, __LINE__ )
#define g_list_free_1(llink)	debug_g_list_free_1( llink, __FILE__, __LINE__ )
#define g_list_free(list)	debug_g_list_free( list, __FILE__, __LINE__ )
#define g_node_new(data)	debug_g_node_new( data, __FILE__, __LINE__ )
#define g_node_destroy(node)	debug_g_node_destroy( node, __FILE__, __LINE__ )


void debug_init( void );
void debug_show_mem_totals( void );
void debug_show_mem_summary( void );
void debug_show_mem_stats( void );
void *debug_malloc( size_t size, const char *source_file, int source_line );
void *debug_realloc( void *block, size_t size, const char *source_file, int source_line );
void *debug_memdup( void *mem, size_t size, const char *source_file, int source_line );
char *debug_strdup( const char *string, const char *source_file, int source_line );
char *debug_strredup( char *old_string, const char *string, const char *source_file, int source_line );
void debug_free( void *block, const char *source_file, int source_line );
void debug_ufree( void *block );

GList *debug_g_list_alloc( const char *src_file, int src_line );
GList *debug_g_list_prepend( GList *list, gpointer data, const char *src_file, int src_line );
GList *debug_g_list_append( GList *list, gpointer data, const char *src_file, int src_line );
GList *debug_g_list_insert( GList *list, gpointer data, gint position, const char *src_file, int src_line );
GList *debug_g_list_insert_sorted( GList *list, gpointer data, GCompareFunc func, const char *src_file, int src_line );
GList *debug_g_list_remove( GList *list, gpointer data, const char *src_file, int src_line );
GList *debug_g_list_copy( GList *list, const char *src_file, int src_line );
void debug_g_list_free_1( GList *llink, const char *src_file, int src_line );
void debug_g_list_free( GList *list, const char *src_file, int src_line );
GNode *debug_g_node_new( gpointer data, const char *src_file, int src_line );
void debug_g_node_destroy( GNode *node, const char *src_file, int src_line );


/* end debug.h */
