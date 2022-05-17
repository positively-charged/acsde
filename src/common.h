#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#if defined( _WIN32 ) || defined( _WIN64 )
#   define OS_WINDOWS 1
#else
#   define OS_WINDOWS 0
#endif

extern const char* c_version;

void mem_init( void );
void* mem_alloc( size_t );
void* mem_realloc( void*, size_t );
void* mem_slot_alloc( size_t );
void mem_free( void* );
void mem_free_all( void );

#define ARRAY_SIZE( a ) ( sizeof( a ) / sizeof( a[ 0 ] ) )
#define STATIC_ASSERT( ... ) \
  STATIC_ASSERT_REAL( __VA_ARGS__,, )
#define STATIC_ASSERT_REAL( cond, msg, ... ) \
   extern int STATIC_ASSERT__##msg[ !! ( cond ) ]
#define UNREACHABLE() printf( \
   "%s:%d: internal error: unreachable code\n", \
   __FILE__, __LINE__ );

// Make sure the data types are of sizes we want.
STATIC_ASSERT( CHAR_BIT == 8, CHAR_BIT_must_be_8 );
STATIC_ASSERT( sizeof( char ) == 1, char_must_be_1_byte );
STATIC_ASSERT( sizeof( short ) == 2, short_must_be_2_bytes );
STATIC_ASSERT( sizeof( int ) == 4, int_must_be_4_bytes );
STATIC_ASSERT( sizeof( long long ) == 8, long_long_must_be_8_bytes );

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

struct str {
   char* value;
   i32 length;
   i32 buffer_length;
};

void str_init( struct str* );
void str_deinit( struct str* );
void str_copy( struct str*, const char* value, i32 length );
void str_grow( struct str*, i32 length );
void str_append( struct str*, const char* cstr );
void str_append_sub( struct str*, const char* cstr, i32 length );
void str_append_number( struct str*, i32 number );
void str_append_format( struct str* str, const char* format, va_list* args );
void str_clear( struct str* );

// Singly linked list
// --------------------------------------------------------------------------

struct list_link {
   struct list_link* next;
   void* data;
};

struct list {
   struct list_link* head;
   struct list_link* tail;
   u32 size;
};

struct list_iter {
   struct list_link* prev;
   struct list_link* link;
};

void list_init( struct list* list );
u32 list_size( struct list* list );
void* list_head( struct list* list );
void* list_tail( struct list* list );
void list_append( struct list* list, void* data );
void list_prepend( struct list* list, void* data );
void list_iterate( struct list* list, struct list_iter* iter );
bool list_end( struct list_iter* iter );
void list_next( struct list_iter* iter );
void* list_data( struct list_iter* iter );
void list_insert_after( struct list* list,
   struct list_iter* iter, void* data );
void list_insert_before( struct list* list,
   struct list_iter* iter, void* data );
void* list_replace( struct list* list,
   struct list_iter* iter, void* data );
void list_merge( struct list* receiver, struct list* giver );
void* list_shift( struct list* list );
void list_deinit( struct list* list );

// --------------------------------------------------------------------------

#if OS_WINDOWS

#include <windows.h>
#include <time.h>

// NOTE: Volume information is not included. Maybe add it later.
struct fileid {
   int id_high;
   int id_low;
};

#define NEWLINE_CHAR "\r\n"
#define OS_PATHSEP "\\"

struct fs_query {
   WIN32_FILE_ATTRIBUTE_DATA attrs;
   const char* path;
   DWORD err;
   bool attrs_obtained;
};

struct fs_timestamp {
   time_t value;
};

#define strcasecmp _stricmp

#else

#include <sys/types.h>
#include <sys/stat.h>

struct fileid {
   dev_t device;
   ino_t number;
};

#define NEWLINE_CHAR "\n"
#define OS_PATHSEP "/"

struct fs_query {
   const char* path;
   struct stat stat;
   int err;
   bool stat_obtained;
};

struct fs_timestamp {
   time_t value;
};

#endif

struct file_contents {
   char* data;
   int err;
   bool obtained;
};

struct fs_result {
   int err;
};

bool c_read_fileid( struct fileid*, const char* path );
bool c_same_fileid( struct fileid*, struct fileid* );
bool c_read_full_path( const char* path, struct str* );
void c_extract_dirname( struct str* );

int alignpad( int size, int align_size );

void fs_init_query( struct fs_query* query, const char* path );
bool fs_exists( struct fs_query* query );
bool fs_is_dir( struct fs_query* query );
bool fs_get_mtime( struct fs_query* query, struct fs_timestamp* timestamp );
bool fs_create_dir( const char* path, struct fs_result* result );
const char* fs_get_tempdir( void );
void fs_get_file_contents( const char* path, struct file_contents* contents );
void fs_strip_trailing_pathsep( struct str* path );
bool fs_delete_file( const char* path );
bool c_is_absolute_path( const char* path );

#endif
