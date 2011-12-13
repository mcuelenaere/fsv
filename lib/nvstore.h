/* nvstore.h */

/* Nonvolatile storage library */

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


#ifdef __NVSTORE_H__
#error ** multiple inclusion **
#else
#define __NVSTORE_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Boolean type used by NVStore */
#define NVS_BOOL int


/* NVStore instance (opaque type) */
typedef struct _NVStore NVStore;
struct _NVStore;


NVStore *	nvs_open		( const char *filename );
NVS_BOOL	nvs_close		( NVStore *nvs );

void		nvs_change_path		( NVStore *nvs, const char *path );
void		nvs_delete_recursive	( NVStore *nvs, const char *path );
void		nvs_vector_begin	( NVStore *nvs );
void		nvs_vector_end		( NVStore *nvs );

NVS_BOOL	nvs_path_present	( NVStore *nvs, const char *path );

NVS_BOOL	nvs_read_boolean	( NVStore *nvs, const char *path );
int		nvs_read_int		( NVStore *nvs, const char *path );
int		nvs_read_int_token	( NVStore *nvs, const char *path, const char **tokens );
double		nvs_read_float		( NVStore *nvs, const char *path );
char *		nvs_read_string		( NVStore *nvs, const char *path );

NVS_BOOL	nvs_read_boolean_default	( NVStore *nvs, const char *path, NVS_BOOL default_val );
int		nvs_read_int_default		( NVStore *nvs, const char *path, int default_val );
int		nvs_read_int_token_default	( NVStore *nvs, const char *path, const char **tokens, int default_val );
double		nvs_read_float_default		( NVStore *nvs, const char *path, double default_val );
char *		nvs_read_string_default		( NVStore *nvs, const char *path, const char *default_string );

void		nvs_write_boolean	( NVStore *nvs, const char *path, NVS_BOOL val );
void		nvs_write_int		( NVStore *nvs, const char *path, int val );
void		nvs_write_int_token	( NVStore *nvs, const char *path, int val, const char **tokens );
void		nvs_write_float		( NVStore *nvs, const char *path, double val );
void		nvs_write_string	( NVStore *nvs, const char *path, const char *string );


#ifdef __cplusplus
}
#endif

#endif /* not __NVSTORE_H__ */


/* end nvstore.h */
