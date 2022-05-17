/**
 * Stage 1 -- Loading
 * 
 * In the "Loading" stage, we read the object file data into data structures
 * useful for the later stages.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "task.h"
#include "pcode.h"

enum { DEFAULT_SCRIPT_VARS = 20 };

struct loader {
   struct task* task;
   const u8* object_data;
   const u8* object_data_end;
   u32 object_size;
   u32 format;
   u32 directory_offset;
   u32 chunk_offset;
   u32 chunk_end;
   u32 string_offset;
   u32 end_offset;
   bool indirect_format;
   bool small_code;
};

struct pcode_reading {
   const u8* data;
   const u8* data_end;
   struct pcode* head;
   struct pcode* tail;
   u32 obj_pos;
   i32 opcode;
};

struct patch {
   struct pcode* begin;
   struct pcode* end;
};

struct chunk {
   const char* name;
   const u8* data;
   const u8* start;
   const u8* end;
   u32 size;
   bool found;
};

static void init_loader( struct loader* loader, struct task* task );
static void load_module( struct loader* loader );
static void read_file( struct loader* loader );
static bool read_file_data( struct loader* loader, FILE* fh );
static void determine_format( struct loader* loader );
static void expect_data( struct loader* loader, const u8* start, u32 size );
static void expect_offset_in_object_file( struct loader* loader, u32 offset );
static u32 data_left( struct loader* loader, const u8* data );
static bool offset_in_range( struct loader* loader, const u8* start,
   const u8* end, u32 offset );
static bool offset_in_object_file( struct loader* loader, u32 offset );
static void read_object( struct loader* loader );
static void read_acse_object( struct loader* loader );
static void read_scripts( struct loader* loader );
static void read_sptr( struct loader* loader );
static struct script* alloc_script( void );
static void read_sflg( struct loader* loader );
static u32 set_script_flag( struct script* script, u32 flags, u32 flag );
static void read_script_space( struct loader* loader );
static void read_svct( struct loader* loader );
static void reserve_script_space( struct script* script, u32 num_vars );
static void reserve_default_script_space( struct loader* loader );
static void read_script_names( struct loader* loader );
static bool chunk_offset_in_chunk( struct chunk* chunk, u32 offset );
static void read_funcs( struct loader* loader );
static void read_func( struct loader* loader );
static void read_fnam( struct loader* loader );
static void append_object( struct loader* loader, struct node* object );
static u32 object_offset( struct node* node );
static void determine_end_of_objects( struct loader* loader );
static void read_func_body_list( struct loader* loader );
static void read_func_body( struct loader* loader, struct func* func );
static void read_strings( struct loader* loader );
static void reserve_strings( struct loader* loader, u32 num_strings );
static u8 decrypt_ch( u32 string_offset, u32 ch_pos, u8 ch );
static void expect_chunk_data( struct loader* loader, struct chunk* chunk,
   const u8* start, u32 size );
static u32 chunk_data_left( struct chunk* chunk, const u8* data );
static void read_map_vars( struct loader* loader );
static void read_mini( struct loader* loader );
static void read_aray( struct loader* loader );
static void read_aini( struct loader* loader );
static void read_aini_chunk( struct loader* loader, struct chunk* chunk );
static void diag_abort_aini( struct loader* loader, struct chunk* chunk,
   u32 index );
static void reserve_unspecified_vars( struct loader* loader );
static void read_local_arrays( struct loader* loader );
static void read_sary( struct loader* loader );
static void read_local_array_chunk( struct loader* loader,
   struct chunk* chunk );
static void reserve_script_arrays( struct script* script, u32 count );
static void read_fary( struct loader* loader );
static void read_fary_chunk( struct loader* loader, struct chunk* chunk );
static void reserve_func_arrays( struct func* func, u32 count );
static void read_mexp( struct loader* loader );
static void determine_library_name( struct loader* loader );
static void read_load( struct loader* loader );
static void read_mimp( struct loader* loader );
static void read_aimp( struct loader* loader );
static void read_mstr( struct loader* loader );
static void read_astr( struct loader* loader );
static void read_zero_object( struct loader* loader );
static void read_script_list( struct loader* loader );
static void read_script( struct loader* loader );
static void read_string_table( struct loader* loader );
static void read_script_pcode( struct loader* loader, struct script* script );
static void read_script_body_list( struct loader* loader );
static void read_script_body( struct loader* loader, struct script* script );
static void init_pcode_reading( struct pcode_reading* reading, const u8* data,
   const u8* data_end );
static void append_pcode( struct pcode_reading* reading,
   struct pcode* pcode );
static void read_pcode_list( struct loader* loader,
   struct pcode_reading* reading );
static bool have_pcode( struct pcode_reading* reading );
static void read_pcode( struct loader* loader, struct pcode_reading* reading );
static void read_opcode( struct loader* loader,
   struct pcode_reading* reading );
static void read_arg( struct pcode_reading* reading, i32* value );
static void read_jump( struct pcode_reading* reading );
static void read_casejump( struct pcode_reading* reading );
static void read_sortedcasejump( struct loader* loader,
   struct pcode_reading* reading );
static void append_casejump( struct sortedcasejump_pcode* jump,
   struct casejump_pcode* case_jump );
static void read_pushbytes( struct loader* loader,
   struct pcode_reading* reading );
static void read_generic( struct loader* loader,
   struct pcode_reading* reading );
static struct generic_pcode* alloc_generic_pcode( i32 opcode, u32 obj_pos );
static void read_generic_arg( struct loader* loader,
   struct pcode_reading* reading, struct generic_pcode* generic,
   i32 arg_number );
static struct generic_pcode_arg* alloc_generic_pcode_arg( void );
static void append_arg( struct generic_pcode* generic,
   struct generic_pcode_arg* arg );
static void init_pcode( struct pcode* pcode, i32 opcode );
static void patch( struct task* task );
static void patch_scripts( struct task* task );
static void patch_funcs( struct task* task );
static void init_patch( struct patch* patch, struct pcode* begin,
   struct pcode* end );
static void patch_script( struct patch* patch );
static void patch_jump( struct patch* patch, struct jump_pcode* jump );
static void patch_casejump( struct patch* patch,
   struct casejump_pcode* jump );
static void patch_sortedcasejump( struct patch* patch,
   struct sortedcasejump_pcode* jump );
static struct pcode* find_destination( struct patch* patch,
   struct pcode* jump, i32 obj_pos );
static void show_script( struct task* task, struct script* script );
static void diag( struct loader* loader, u32 flags, ... );
static void bail( struct loader* loader );

/**
 * Performs the Loading stage.
 */
void t_load( struct task* task ) {
   struct loader loader;
   init_loader( &loader, task );
   load_module( &loader );
}

static void init_loader( struct loader* loader, struct task* task ) {
   loader->task = task;
   loader->object_data = NULL;
   loader->object_data_end = NULL;
   loader->object_size = 0;
   loader->format = FORMAT_UNKNOWN;
   loader->directory_offset = 0;
   loader->chunk_offset = 0;
   loader->chunk_end = 0;
   loader->string_offset = 0;
   loader->end_offset = 0;
   loader->indirect_format = false;
   loader->small_code = false;
}

static void load_module( struct loader* loader ) {
   read_file( loader );
   determine_format( loader );
   read_object( loader );
   patch( loader->task );
}

static void read_file( struct loader* loader ) {
   FILE* fh = fopen( loader->task->options->object_file, "rb" );
   if ( ! fh ) {
      diag( loader, DIAG_ERR,
         "failed to open object file: \"%s\"",
         loader->task->options->object_file );
      t_bail( loader->task );
   }
   bool success = read_file_data( loader, fh );
   fclose( fh );
   if ( ! success ) {
      bail( loader );
   }
}

static bool read_file_data( struct loader* loader, FILE* fh ) {
   fseek( fh, 0, SEEK_END );
   long tell_result = ftell( fh );
   if ( ! ( tell_result >= 0 ) ) {
      diag( loader, DIAG_ERR,
         "failed to seek to end of object file" );
      return false;
   }
   size_t size = ( size_t ) tell_result;
   i32 seek_result = fseek( fh, 0, SEEK_SET );
   if ( seek_result != 0 ) {
      diag( loader, DIAG_ERR,
         "failed to seek to beginning of object file" );
      return false;
   }
   STATIC_ASSERT( SIZE_MAX > UINT_MAX );
   if ( size > UINT_MAX ) {
      diag( loader, DIAG_ERR,
         "object file is %zu bytes, but the maximum supported object file "
         "size is %d bytes", size, UINT_MAX );
      return false;
   }
   u32 object_size = ( u32 ) size;
   if ( object_size > 0 ) {
      u8* data = mem_alloc( sizeof( data[ 0 ] ) * object_size );
      size_t num_read = fread( data, sizeof( data[ 0 ] ), object_size, fh );
      if ( num_read != object_size ) {
         diag( loader, DIAG_ERR,
            "failed to read %zu byte of object file", object_size - num_read );
         return false;
      }
      loader->object_data = data;
      loader->object_data_end = data + object_size;
      loader->object_size = object_size;
   }
   return true;
}

static void determine_format( struct loader* loader ) {
   struct {
      char id[ 4 ];
      u32 offset;
   } header;
   const u8* data = loader->object_data;
   if ( data_left( loader, data ) < sizeof( header ) ) {
      diag( loader, DIAG_ERR,
         "object file too small to be an ACS object file" );
      bail( loader );
   }
   memcpy( &header, data, sizeof( header ) );
   loader->directory_offset = header.offset;
   if ( memcmp( header.id, "ACSE", 4 ) == 0 ||
      memcmp( header.id, "ACSe", 4 ) == 0 ) {
      loader->format = ( header.id[ 3 ] == 'E' ) ?
            FORMAT_BIGE : FORMAT_LITTLEE;
      loader->chunk_offset = header.offset;
      loader->chunk_end = ( u32 )
         ( loader->object_data_end - loader->object_data );
   }
   else if ( memcmp( header.id, "ACS\0", 4 ) == 0 ) {
      u32 offset = ( u32 ) ( header.offset - sizeof( header.id ) );
      expect_offset_in_object_file( loader, offset );
      data = loader->object_data + offset;
      expect_data( loader, data, sizeof( header.id ) );
      if ( memcmp( data, "ACSE", 4 ) == 0 ||
         memcmp( data, "ACSe", 4 ) == 0 ) {
         loader->format = ( data[ 3 ] == 'E' ) ?
            FORMAT_BIGE : FORMAT_LITTLEE;
         loader->chunk_end = ( u32 ) ( offset - sizeof( header.offset ) );
         memcpy( &loader->chunk_offset, data - sizeof( u32 ), sizeof( i32 ) );
         loader->indirect_format = true;
      }
      else {
         loader->format = FORMAT_ZERO;
      }
   }
   else {
      t_diag( loader->task, DIAG_ERR,
         "object file of unknown format" );
      t_bail( loader->task );
   }
   if ( loader->format == FORMAT_LITTLEE ) {
      loader->task->compact = true;
      loader->small_code = true;
   }
}

static u32 data_left( struct loader* loader, const u8* data ) {
   return ( u32 ) ( loader->object_data_end - data );
}

static bool offset_in_range( struct loader* loader, const u8* start,
   const u8* end, u32 offset ) {
   return ( offset >= ( start - loader->object_data ) &&
      offset < ( end - loader->object_data ) );
}

static bool offset_in_object_file( struct loader* loader, u32 offset ) {
   return offset_in_range( loader, loader->object_data,
      loader->object_data_end, offset );
}

static void expect_data( struct loader* loader, const u8* start, u32 size ) {
   u32 left = data_left( loader, start );
   if ( left < size ) {
      diag( loader, DIAG_ERR,
         "expecting to read %u byte%s, "
         "but object file has %u byte%s of data left to read",
         size, size == 1u ? "" : "s",
         left, left == 1u ? "" : "s" );
      bail( loader );
   }
}

static void expect_offset_in_object_file( struct loader* loader, u32 offset ) {
   if ( ! offset_in_object_file( loader, offset ) ) {
      diag( loader, DIAG_ERR,
         "object file gives an offset that points beyond the boundaries of "
         "the object file" );
      bail( loader );
   }
}

static void read_object( struct loader* loader ) {
   switch ( loader->format ) {
   case FORMAT_BIGE:
   case FORMAT_LITTLEE:
      read_acse_object( loader );
      break;
   case FORMAT_ZERO:
      read_zero_object( loader );
      break;
   case FORMAT_UNKNOWN:
      break;
   }
}

static void read_acse_object( struct loader* loader ) {
   const u8* data = loader->object_data + loader->directory_offset;
   i32 num_scripts = 0;
   memcpy( &num_scripts, data, sizeof( num_scripts ) );
   data += sizeof( num_scripts );
   if ( num_scripts > 0 ) {
      struct {
         u32 number;
         u32 offset;
         u32 num_param;
      } entry;
      memcpy( &entry, data, sizeof( entry ) );
      loader->end_offset = entry.offset;
   }
   else {
      loader->end_offset = loader->chunk_offset;
   }
   read_scripts( loader );
   read_funcs( loader );
   determine_end_of_objects( loader );
   read_script_body_list( loader );
   read_func_body_list( loader );
   read_strings( loader );
   read_map_vars( loader );
   read_local_arrays( loader );
   read_mexp( loader );
   read_load( loader );
   read_mimp( loader );
   read_aimp( loader );
   read_mstr( loader );
   read_astr( loader );
   // #nowadauthor creates a dummy directory that is empty for BEHAVIOR lumps.
   if ( ! loader->task->importable && num_scripts > 0 ) {
      loader->task->wadauthor = true;
   }
}

static void init_chunk( struct chunk* chunk, const char* name ) {
   chunk->name = name;
   chunk->data = NULL;
   chunk->size = 0;
   chunk->found = false;
} 

static void find_chunk( struct loader* loader, struct chunk* chunk ) {
   const u8* data = ( const u8* ) loader->object_data + loader->chunk_offset;
   const u8* data_end = ( const u8* ) loader->object_data + loader->chunk_end;
   while ( data < data_end ) {
      struct {
         char name[ 4 ];
         u32 size;
      } header;
      memcpy( &header, data, sizeof( header ) );
      data += sizeof( header );
      if ( memcmp( header.name, chunk->name, sizeof( header.name ) ) == 0 ) {
         chunk->size = header.size;
         chunk->data = data;
         chunk->start = data;
         chunk->end = chunk->data + chunk->size;
         chunk->found = true;
         return;
      }
      data += header.size;
   }
   chunk->found = false;
}

static void find_chunk_from_offset( struct loader* loader,
   struct chunk* chunk, const u8* offset ) {
   const u8* data = offset ? offset :
      ( const u8* ) loader->object_data + loader->chunk_offset;
   const u8* data_end = ( const u8* ) loader->object_data + loader->chunk_end;
   while ( data < data_end ) {
      struct {
         char name[ 4 ];
         u32 size;
      } header;
      memcpy( &header, data, sizeof( header ) );
      data += sizeof( header );
      if ( memcmp( header.name, chunk->name, sizeof( header.name ) ) == 0 ) {
         chunk->size = header.size;
         chunk->data = data;
         chunk->start = data;
         chunk->end = chunk->data + chunk->size;
         chunk->found = true;
         return;
      }
      data += header.size;
   }
   chunk->found = false;
}

static void next_chunk( struct loader* loader, struct chunk* chunk ) {
   find_chunk_from_offset( loader, chunk, chunk->data + chunk->size );
}

static void read_scripts( struct loader* loader ) {
   read_sptr( loader );
   read_sflg( loader );
   read_script_space( loader );
   read_script_names( loader );
}

static void read_sptr( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "SPTR" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   struct {
      i16 number;
      u8 type;
      u8 num_param;
      u32 offset;
   } entry;
   const u8* data = chunk.start;
   u32 count = chunk.size / sizeof( entry );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      struct script* script = alloc_script();
      script->number = entry.number;
      script->type = entry.type;
      script->num_param = entry.num_param;
      script->offset = entry.offset;
      list_append( &loader->task->scripts, script );
      append_object( loader, &script->node );
   }
}

static struct script* alloc_script( void ) {
   struct script* script = mem_alloc( sizeof( *script ) );
   script->node.type = NODE_SCRIPT;
   script->body_start = NULL;
   script->body_end = NULL;
   script->vars = NULL;
   str_init( &script->name );
   script->number = 0;
   script->offset = 0;
   script->end_offset = 0;
   script->num_param = 0;
   script->type = SCRIPT_TYPE_CLOSED;
   script->flags = 0;
   script->num_vars = 0;
   script->body = NULL;
   script->named_script = false;
   return script;
}

static void read_sflg( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "SFLG" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   struct {
      i16 number;
      u16 flags;
   } entry;
   const u8* data = chunk.start;
   u32 count = chunk.size / sizeof( entry );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      struct script* script = t_find_script( loader->task, entry.number );
      if ( script ) {
         u32 flags = entry.flags;
         flags = set_script_flag( script, flags, SCRIPTFLAG_NET );
         flags = set_script_flag( script, flags, SCRIPTFLAG_CLIENTSIDE );
         if ( flags != 0u ) {
            diag( loader, DIAG_WARN,
               "script %d contains at least one unknown script flag",
               script->number );
         }
      }
      else {
         diag( loader, DIAG_WARN,
            "%s chunk has an entry for script %d, but there is no such script",
            chunk.name, entry.number );
      }
   }
}

static u32 set_script_flag( struct script* script, u32 flags, u32 flag ) {
   if ( flags & flag ) {
      script->flags |= flag;
      flags &= ~flag;
   }
   return flags;
}

struct script* t_find_script( struct task* task, i32 number ) {
   struct list_iter i;
   list_iterate( &task->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( script->number == number ) {
         return script;
      }
      list_next( &i );
   }
   return NULL;
}

struct func* t_find_func( struct task* task, u32 index ) {
   struct list_iter i;
   list_iterate( &task->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      if ( func->more.user->index == index ) {
         return func;
      }
      list_next( &i );
   }
   return NULL;
}

static void read_script_space( struct loader* loader ) {
   read_svct( loader );
   reserve_default_script_space( loader );
}

static void read_svct( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "SVCT" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   struct {
      i16 number;
      u16 size;
   } entry;
   const u8* data = chunk.start;
   u32 count = chunk.size / sizeof( entry );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      struct script* script = t_find_script( loader->task, entry.number );
      if ( script ) {
         reserve_script_space( script, entry.size );
      }
      else {
         diag( loader, DIAG_WARN,
            "%s chunk has an entry for script %d, but there is no such script",
            chunk.name, entry.number );
      }
   }
}

static void reserve_script_space( struct script* script, u32 num_vars ) {
   script->vars = mem_alloc( sizeof( script->vars[ 0 ] ) * num_vars );
   script->num_vars = num_vars;
   for ( u32 i = 0; i < num_vars; ++i ) {
      script->vars[ i ] = NULL;
   }
}

static void reserve_default_script_space( struct loader* loader ) {
   struct list_iter i;
   list_iterate( &loader->task->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      if ( ! script->vars ) {
         reserve_script_space( script, DEFAULT_SCRIPT_VARS );
      }
      list_next( &i );
   }
}

static void read_script_names( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "SNAM" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.start;
   u32 count = 0;
   expect_chunk_data( loader, &chunk, data, sizeof( count ) );
   memcpy( &count, data, sizeof( count ) );
   data += sizeof( count );
   i32 script_number = -1;
   if ( chunk_data_left( &chunk, data ) < sizeof( u32 ) * count ) {
      diag( loader, DIAG_ERR,
         "%s chunk gives a number of script name offsets (%d) that cannot "
         "possibly fit in the chunk", chunk.name, count );
      bail( loader );
   }
   for ( u32 i = 0; i < count; ++i ) {
      u32 offset = 0;
      memcpy( &offset, data, sizeof( offset ) );
      data += sizeof( offset );
      if ( ! chunk_offset_in_chunk( &chunk, offset ) ) {
         diag( loader, DIAG_ERR,
            "string offset in position %d of %s chunk points outside of "
            "chunk data range", i, chunk.name );
         bail( loader );
      }
      struct script* script = t_find_script( loader->task, script_number );
      if ( script ) {
         str_append( &script->name, ( const char* ) chunk.data + offset );
         script->named_script = true;
      }
      --script_number;
   }
}

static void read_funcs( struct loader* loader ) {
   read_func( loader );
   read_fnam( loader );
}

static void read_func( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "FUNC" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   struct {
      u8 params;
      u8 size;
      u8 value;
      u8 padding;
      u32 offset;
   } entry;
   const u8* data = chunk.data;
   u32 count = chunk.size / sizeof( entry );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      struct func* func = t_alloc_func();
      func->type = FUNC_USER;
      func->min_param = entry.params;
      func->max_param = entry.params;
      if ( entry.value != 0 ) {
         func->return_spec = SPEC_INT;
      }
      struct func_user* user = mem_alloc( sizeof( *user ) );
      user->offset = entry.offset;
      user->end_offset = entry.offset;
      user->index = i;
      func->more.user = user;
      user->num_vars = ( u32 ) entry.params + entry.size;
      if ( user->num_vars > 0 ) {
         user->vars = mem_alloc( sizeof( user->vars[ 0 ] ) *
            ( size_t ) user->num_vars );
         for ( u32 i = 0; i < user->num_vars; ++i ) {
            user->vars[ i ] = NULL;
         }
      }
      list_append( &loader->task->funcs, func );
      append_object( loader, &func->node );
   }
}

static void read_fnam( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "FNAM" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.data;
   i32 count = 0;
   memcpy( &count, data, sizeof( count ) );
   data += sizeof( count );
   struct list_iter i;
   list_iterate( &loader->task->funcs, &i );
   i32 index = 0;
   while ( ! list_end( &i ) && index < count ) {
      i32 offset = 0;
      memcpy( &offset, data, sizeof( offset ) );
      data += sizeof( offset );
      struct func* func = list_data( &i );
      str_append( &func->name, ( const char* ) chunk.data + offset );
      list_next( &i );
      ++index;
   }
}

static void append_object( struct loader* loader, struct node* object ) {
   struct list_iter i;
   list_iterate( &loader->task->objects, &i );
   while ( ! list_end( &i ) &&
      object_offset( object ) > object_offset( list_data( &i ) ) ) {
      list_next( &i );
   }
   list_insert_before( &loader->task->objects, &i, object );
}

static u32 object_offset( struct node* node ) { 
   if ( node->type == NODE_SCRIPT ) {
      struct script* script = ( struct script* ) node;
      return script->offset;
   }
   else if ( node->type == NODE_FUNC ) {
      struct func* func = ( struct func* ) node;
      return func->more.user->offset;
   }
   else {
      return 0;
   }
}

static void determine_end_of_objects( struct loader* loader ) {
{
   struct list_iter i;
   list_iterate( &loader->task->objects, &i );
   while ( ! list_end( &i ) ) {
      //printf( "%d\n", object_offset( list_data( &i ) ) );
      list_next( &i );
   }
}
   
   struct list_iter i;
   list_iterate( &loader->task->objects, &i );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      list_next( &i );
      u32 end_offset = loader->end_offset;
      if ( ! list_end( &i ) ) {
         end_offset = object_offset( list_data( &i ) );
      }
      if ( node->type == NODE_SCRIPT ) {
         struct script* script = ( struct script* ) node;
         script->end_offset = end_offset;
      }
      else if ( node->type == NODE_FUNC ) {
         struct func* func = ( struct func* ) node;
         func->more.user->end_offset = end_offset;
      }
      else {
         break;
      }
//printf( "%d %d\n", object_offset( node ), end_offset );
   }
}

static void read_func_body_list( struct loader* loader ) {
   struct list_iter i;
   list_iterate( &loader->task->funcs, &i );
   while ( ! list_end( &i ) ) {
      read_func_body( loader, list_data( &i ) );
      list_next( &i );
   }
}

static void read_func_body( struct loader* loader, struct func* func ) {
   const u8* data = loader->object_data + func->more.user->offset;
   const u8* data_end = loader->object_data + func->more.user->end_offset;
   struct pcode_reading reading;
   init_pcode_reading( &reading, data, data_end );
   read_pcode_list( loader, &reading );
   func->more.user->start = reading.head;
   func->more.user->end = reading.tail->prev;
/*
   struct pcode* pcode = script->body_start;
   while ( pcode != script->body_end->next ) {
      printf( "%d\n", pcode->opcode );
      pcode = pcode->next;
   }
*/
}

static u32 chunk_data_left( struct chunk* chunk, const u8* data ) {
   return ( u32 ) ( chunk->end - data );
}

static bool chunk_offset_in_range( struct chunk* chunk, const u8* start,
   const u8* end, u32 offset ) {
   return ( offset >= ( start - chunk->start ) &&
      offset < ( end - chunk->start ) );
}

static bool chunk_offset_in_chunk( struct chunk* chunk, u32 offset ) {
   return chunk_offset_in_range( chunk, chunk->start, chunk->end, offset );
}

static void read_strings( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "STRL" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      init_chunk( &chunk, "STRE" );
      find_chunk( loader, &chunk );
      if ( ! chunk.found ) {
         return;
      }
      loader->task->encrypt_str = true;
   }
   const u8* data = chunk.start;
   if ( chunk_data_left( &chunk, data ) < sizeof( i32 ) * 3 ) {
      diag( loader, DIAG_ERR,
         "cannot read string-count portion of %s chunk because it is smaller "
         "than expected", chunk.name );
      bail( loader );
   }
   data += sizeof( i32 );
   u32 count = 0;
   memcpy( &count, data, sizeof( count ) );
   data += sizeof( count );
   data += sizeof( i32 );
   if ( chunk_data_left( &chunk, data ) < sizeof( u32 ) * count ) {
      diag( loader, DIAG_ERR,
         "%s chunk gives %d string offsets but is too small to contain "
         "that many offsets", chunk.name, count );
      bail( loader );
   }
   reserve_strings( loader, count );
   for ( u32 i = 0; i < count; ++i ) {
      u32 offset = 0;
      memcpy( &offset, data, sizeof( offset ) );
      data += sizeof( offset );
      if ( ! chunk_offset_in_chunk( &chunk, offset ) ) {
         diag( loader, DIAG_ERR,
            "string offset in position %d of %s chunk points outside of "
            "chunk data range", i, chunk.name );
         bail( loader );
      }
      const u8* start = chunk.start + offset;
      const u8* string_data = start;
      while ( chunk_data_left( &chunk, string_data ) ) {
         u8 ch = *string_data;
         if ( loader->task->encrypt_str ) {
            ch = decrypt_ch( offset, ( u32 ) ( string_data - start ), ch );
         }
         char string_part[] = { ( char ) ch, '\0' };
         str_append( &loader->task->strings[ i ], string_part );
         if ( ! ch ) {
            break;
         }
         ++string_data;
      }
      if ( ! chunk_data_left( &chunk, string_data ) ) {
         diag( loader, DIAG_ERR,
            "unterminated string at offset %d of %s chunk", offset,
            chunk.name );
         bail( loader );
      }
   }
}

static void reserve_strings( struct loader* loader, u32 num_strings ) {
   loader->task->num_strings = num_strings;
   loader->task->strings = mem_alloc(
      sizeof( loader->task->strings[ 0 ] ) * num_strings );
   for ( u32 i = 0; i < num_strings; ++i ) {
      str_init( &loader->task->strings[ i ] );
   }
}

static u8 decrypt_ch( u32 string_offset, u32 ch_pos, u8 ch ) {
   enum { ENCRYPTION_CONSTANT = 157135 };
   ch = ( u8 ) (
      ch ^ ( ( ENCRYPTION_CONSTANT * string_offset ) + ch_pos / 2 ) );
   return ch;
}

static void expect_chunk_data( struct loader* loader, struct chunk* chunk,
   const u8* start, u32 size ) {
   u32 left = chunk_data_left( chunk, start );
   if ( left < size ) {
      diag( loader, DIAG_ERR,
         "expecting to read %d byte%s, "
         "but %s chunk has %d byte%s of data left to read",
         size, size == 1u ? "" : "s", chunk->name,
         left, left == 1u ? "" : "s" );
      bail( loader );
   }
}

static void read_map_vars( struct loader* loader ) {
   read_mini( loader );
   read_aray( loader );
   read_aini( loader );
   reserve_unspecified_vars( loader );
}

static void read_mini( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "MINI" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.start;
   u32 index = 0;
   expect_chunk_data( loader, &chunk, data, sizeof( index ) );
   memcpy( &index, data, sizeof( index ) );
   data += sizeof( index );
   u32 count = ( chunk.size - ( u32 ) sizeof( index ) ) / sizeof( i32 );
   for ( u32 i = 0; i < count; ++i ) {
      struct var* var = t_reserve_map_var( loader->task, index );
      i32 value = 0;
      memcpy( &value, data, sizeof( value ) );
      data += sizeof( value );
      if ( value != 0 ) {
         var->initz = t_alloc_literal_expr( value );
      }
      ++index;
   }
}

static void read_aray( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "ARAY" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   struct {
      u32 index;
      u32 size;
   } entry;
   const u8* data = chunk.start;
   u32 count = chunk.size / ( i32 ) sizeof( entry );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      if ( entry.index >= ARRAY_SIZE( loader->task->map_vars ) ) {
         diag( loader, DIAG_ERR,
            "entry %d of %s chunk specifies an array with index %d, which is "
            "greater than the maximum index %d", i, chunk.name, entry.index,
            ARRAY_SIZE( loader->task->map_vars ) - 1 );
         bail( loader );
      }
      struct var* var = t_reserve_map_var( loader->task, entry.index );
      var->dim_length = entry.size;
      var->array = true;
   }
}

static void read_aini( struct loader* loader ) {
   const u8* offset = NULL;
   while ( true ) {
      struct chunk chunk;
      init_chunk( &chunk, "AINI" );
      find_chunk_from_offset( loader, &chunk, offset );
      if ( ! chunk.found ) {
         return;
      }
      read_aini_chunk( loader, &chunk );
      offset = chunk.data + chunk.size;
   }
}

static void read_aini_chunk( struct loader* loader, struct chunk* chunk ) {
   const u8* data = chunk->start;
   u32 index = 0;
   expect_chunk_data( loader, chunk, data, sizeof( index ) );
   memcpy( &index, data, sizeof( index ) );
   data += sizeof( index );
   if ( index >= ARRAY_SIZE( loader->task->map_vars ) ) {
      diag( loader, DIAG_WARN,
         "%s chunk specifies an array with index %d, which is "
         "greater than the maximum index %d", chunk->name, index,
         ARRAY_SIZE( loader->task->map_vars ) - 1 );
      diag_abort_aini( loader, chunk, index );
      return;
   }
   struct var* var = loader->task->map_vars[ index ];
   if ( ! ( var && var->array ) ) {
      diag( loader, DIAG_WARN,
         "%s chunk specifies an array with index %d, but there is no "
         "such array", chunk->name, index, index );
      diag_abort_aini( loader, chunk, index );
      return;
   }
   u32 count = ( u32 ) ( chunk->size - sizeof( index ) ) / sizeof( i32 );
   if ( count > ( u32 ) var->dim_length ) {
      diag( loader, DIAG_WARN,
         "%s chunk for array %d specifies %d initializers, but array has "
         "%d elements", chunk->name, index, count, var->dim_length );
      diag( loader, DIAG_NOTE,
         "will change size of array %d to %d",
         index, count );
      var->dim_length = count;
   }
   struct value* head = NULL;
   struct value* tail = NULL;
   for ( u32 i = 0; i < count; ++i ) {
      i32 value = 0;
      memcpy( &value, data, sizeof( value ) );
      data += sizeof( value );
      if ( value != 0 ) {
         struct value* value_node = mem_alloc( sizeof( *value_node ) );
         value_node->initial.next = NULL;
         value_node->initial.multi = false;
         value_node->next = NULL;
         value_node->type = VALUE_NUMBER;
         value_node->index = ( i32 ) i;
         value_node->value = value;
         if ( head ) {
            tail->next = value_node;
         }
         else {
            head = value_node;
         }
         tail = value_node;
      }
   }
   var->value = head;
}

static void diag_abort_aini( struct loader* loader, struct chunk* chunk,
   u32 index ) { 
   diag( loader, DIAG_NOTE,
      "will abort reading %s chunk for array %d", chunk->name, index );
}

static void reserve_unspecified_vars( struct loader* loader ) {
   u32 last_var = 0;
   for ( u32 i = 0; i < ARRAY_SIZE( loader->task->map_vars ); ++i ) {
      if ( loader->task->map_vars[ i ] ) {
         last_var = i;
      }
   }
   for ( u32 i = 0; i < last_var; ++i ) {
      if ( ! loader->task->map_vars[ i ] ) {
         t_reserve_map_var( loader->task, i );
      }
   }
}

static void read_local_arrays( struct loader* loader ) {
   read_sary( loader );
   read_fary( loader );
}

static void read_sary( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "SARY" );
   find_chunk( loader, &chunk );
   while ( chunk.found ) {
      read_local_array_chunk( loader, &chunk );
      next_chunk( loader, &chunk );
   }
}

static void read_local_array_chunk( struct loader* loader,
   struct chunk* chunk ) {
   const u8* data = chunk->start;
   short number = 0;
   expect_chunk_data( loader, chunk, data, sizeof( number ) );
   memcpy( &number, data, sizeof( number ) );
   data += sizeof( number );
   struct script* script = t_find_script( loader->task, number );
   if ( ! script ) {
      diag( loader, DIAG_WARN,
         "found %s chunk for script %d, but there is no such script",
         chunk->name, number );
      diag( loader, DIAG_NOTE,
         "will abort reading %s chunk for script %d", chunk->name, number );
      return;
   }
   u32 count = ( u32 ) ( chunk->size - sizeof( number ) ) / sizeof( i32 );
   if ( count == 0 ) {
      return;
   }
   reserve_script_arrays( script, count );
   for ( u32 i = 0; i < count; ++i ) {
      u32 size = 0;
      memcpy( &size, data, sizeof( size ) );
      data += sizeof( size );
      struct var* var = t_alloc_var();
      var->dim_length = size;
      var->index = i;
      var->array = true;
      script->arrays[ i ] = var;
   }
}

static void reserve_script_arrays( struct script* script, u32 count ) {
   script->arrays = mem_alloc( sizeof( script->arrays[ 0 ] ) * count );
   for ( u32 i = 0; i < count; ++i ) {
      script->arrays[ i ] = NULL;
   }
}

static void read_fary( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "FARY" );
   find_chunk( loader, &chunk );
   while ( chunk.found ) {
      read_fary_chunk( loader, &chunk );
      next_chunk( loader, &chunk );
   }
}

static void read_fary_chunk( struct loader* loader,
   struct chunk* chunk ) {
   const u8* data = chunk->start;
   u16 index = 0;
   expect_chunk_data( loader, chunk, data, sizeof( index ) );
   memcpy( &index, data, sizeof( index ) );
   data += sizeof( index );
   struct func* func = t_find_func( loader->task, index );
   if ( ! func ) {
      diag( loader, DIAG_WARN,
         "found %s chunk for function %d, but there is no such function",
         chunk->name, index );
      diag( loader, DIAG_NOTE,
         "will abort reading %s chunk for function %d", chunk->name, index );
      return;
   }
   u32 count = ( u32 ) ( chunk->size - sizeof( index ) ) / sizeof( i32 );
   if ( count == 0 ) {
      return;
   }
   reserve_func_arrays( func, count );
   for ( u32 i = 0; i < count; ++i ) {
      u32 size = 0;
      memcpy( &size, data, sizeof( size ) );
      data += sizeof( size );
      struct var* var = t_alloc_var();
      var->dim_length = size;
      var->index = i;
      var->array = true;
      func->more.user->arrays[ i ] = var;
   }
}

static void reserve_func_arrays( struct func* func, u32 count ) {
   func->more.user->arrays = mem_alloc(
      sizeof( func->more.user->arrays[ 0 ] ) * count );
   for ( u32 i = 0; i < count; ++i ) {
      func->more.user->arrays[ i ] = NULL;
   }
}

static void read_mexp( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "MEXP" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.data;
   u32 count = 0;
   memcpy( &count, data, sizeof( count ) );
   data += sizeof( count );
   for ( u32 i = 0; i < count; ++i ) {
      u32 offset = 0;
      memcpy( &offset, data, sizeof( offset ) );
      struct var* var = t_reserve_map_var( loader->task, i );
      if ( offset ) {
         str_append( &var->name, ( const char* ) ( chunk.data + offset ) );
      }
      data += sizeof( offset );
   }
   loader->task->importable = true;
   determine_library_name( loader );
}

static void determine_library_name( struct loader* loader ) {
   const char* filename = strrchr( loader->task->options->object_file,
      OS_PATHSEP[ 0 ] );
   if ( filename ) {
      size_t length = strcspn( filename + 1, "." );
      str_append_sub( &loader->task->library_name, filename + 1,
         ( i32 ) length );
   }
}

static void read_load( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "LOAD" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   u32 i = 0;
   u32 start = 0;
   while ( i < chunk.size ) {
      if ( chunk.data[ i ] == '\0' ) {
         struct imported_module* module = mem_alloc( sizeof( *module ) );
         str_init( &module->name );
         str_append_sub( &module->name,
            ( const char* ) ( chunk.data + start ), ( i32 ) ( i - start ) );
         list_append( &loader->task->imports, module );
         start = i + 1;
      }
      ++i;
   }
}

static void read_mimp( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "MIMP" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   u32 min_entry_size = sizeof( i32 );
   u32 min_size = chunk.size - min_entry_size;
   u32 i = 0;
   while ( i < min_size ) {
      u32 index = 0;
      memcpy( &index, chunk.data + i, sizeof( index ) );
      i += ( u32 ) sizeof( index );
      u32 start = i;
      while ( i < chunk.size && chunk.data[ i ] != '\0' ) {
         ++i;
      }
      if ( i >= chunk.size ) {}
      struct var* var = t_reserve_map_var( loader->task, index );
      if ( var ) {}
      str_append_sub( &var->name, ( const char * )
         ( chunk.data + start ), ( i32 ) ( i - start ) );
      var->imported = true;
      ++i;
   }
}

static void read_aimp( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "AIMP" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   u32 min_entry_size = sizeof( i32 ) * 2;
   u32 min_size = chunk.size - min_entry_size;
   u32 i = 4;
   while ( i < min_size ) {
      u32 index = 0;
      memcpy( &index, chunk.data + i, sizeof( index ) );
      i += ( u32 ) sizeof( index );
      u32 size = 0;
      memcpy( &size, chunk.data + i, sizeof( size ) );
      i += ( u32 ) sizeof( size );
      u32 start = i;
      while ( i < chunk.size && chunk.data[ i ] != '\0' ) {
         ++i;
      }
      if ( i >= chunk.size ) {}
      struct var* var = t_reserve_map_var( loader->task, index );
      if ( var ) {}
      str_append_sub( &var->name, ( const char * )
         ( chunk.data + start ), ( i32 ) ( i - start ) );
      var->dim_length = size;
      var->array = true;
      var->imported = true;
      ++i;
   }
}

static void read_mstr( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "MSTR" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.data;
   u32 index = 0;
   u32 count = chunk.size / sizeof( index );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &index, data, sizeof( index ) );
      data += sizeof( index );
      struct var* var = t_reserve_map_var( loader->task, index );
      var->spec = SPEC_STR;
   }
}

static void read_astr( struct loader* loader ) {
   struct chunk chunk;
   init_chunk( &chunk, "ASTR" );
   find_chunk( loader, &chunk );
   if ( ! chunk.found ) {
      return;
   }
   const u8* data = chunk.data;
   u32 index = 0;
   u32 count = chunk.size / sizeof( index );
   for ( u32 i = 0; i < count; ++i ) {
      memcpy( &index, data, sizeof( index ) );
      data += sizeof( index );
      struct var* var = t_reserve_map_var( loader->task, index );
      struct value* value = var->value;
      while ( value ) {
         value->type = VALUE_STRING;
         value = value->next;
      }
   }
}

static void read_zero_object( struct loader* loader ) {
   read_script_list( loader );
   read_string_table( loader );
   determine_end_of_objects( loader );
   read_script_body_list( loader );
}

// TODO: Read all the scripts before reading their pcodes, so the end of each
// script can be determined.
static void read_script_list( struct loader* loader ) {
   const u8* data = loader->object_data + loader->directory_offset;
   i32 num_scripts = 0;
   memcpy( &num_scripts, data, sizeof( num_scripts ) );
   data += sizeof( num_scripts );
   i32 i = 0;
   while ( i < num_scripts ) {
      struct {
         u32 number;
         u32 offset;
         u32 num_param;
      } entry;
      memcpy( &entry, data, sizeof( entry ) );
      data += sizeof( entry );
      struct script* script = mem_alloc( sizeof( *script ) );
      script->number = ( i32 ) ( entry.number % 1000 );
      script->type = entry.number / 1000;
      script->num_param = entry.num_param;
      script->offset = entry.offset;
      script->end_offset = 0;
      u32 num_vars = 20;
      script->vars = mem_alloc( sizeof( script->vars[ 0 ] ) * num_vars );
      script->num_vars = num_vars;
      for ( u32 i = 0; i < num_vars; ++i ) {
         script->vars[ i ] = NULL;
      }
      // read_script_pcode( load, script );
      list_append( &loader->task->scripts, script );
      ++i;
   }
   loader->string_offset = ( u32 ) ( data - loader->object_data );
   loader->end_offset = loader->directory_offset;
}

static void read_script( struct loader* loader ) {

}

static void read_string_table( struct loader* loader ) {
   const u8* data = loader->object_data + loader->string_offset;
   u32 num_strings = 0;
   memcpy( &num_strings, data, sizeof( num_strings ) );
   data += sizeof( num_strings );
   reserve_strings( loader, num_strings );
   for ( u32 i = 0; i < num_strings; ++i ) {
      u32 offset = 0;
      memcpy( &offset, data, sizeof( offset ) );
      str_append( &loader->task->strings[ i ], ( const char* ) loader->object_data + offset );
      data += sizeof( offset );
      if ( i == 0 ) {
         loader->end_offset = offset;
      }
   }
}

static void read_script_body_list( struct loader* loader ) {
   struct list_iter i;
   list_iterate( &loader->task->scripts, &i );
   while ( ! list_end( &i ) ) {
      read_script_body( loader, list_data( &i ) );
      list_next( &i );
   }
}

static void read_script_body( struct loader* loader, struct script* script ) {
   const u8* data = loader->object_data + script->offset;
   const u8* data_end = loader->object_data + script->end_offset;
   struct pcode_reading reading;
   init_pcode_reading( &reading, data, data_end );
   read_pcode_list( loader, &reading );
   script->body_start = reading.head;
   script->body_end = reading.tail->prev;
/*
   struct pcode* pcode = script->body_start;
   while ( pcode != script->body_end->next ) {
      printf( "%d\n", pcode->opcode );
      pcode = pcode->next;
   }
*/
}

static void init_pcode_reading( struct pcode_reading* reading, const u8* data,
   const u8* data_end ) {
   reading->data = data;
   reading->data_end = data_end;
   reading->head = NULL;
   reading->tail = NULL;
   reading->opcode = PCD_NOP;
   reading->obj_pos = 0;
}

static void append_pcode( struct pcode_reading* reading,
   struct pcode* pcode ) {
   if ( reading->head ) {
      pcode->prev = reading->tail;
      reading->tail->next = pcode;
   }
   else {
      reading->head = pcode;
   }
   reading->tail = pcode;
}

static void read_pcode_list( struct loader* loader,
   struct pcode_reading* reading ) {
   while ( have_pcode( reading ) ) {
      read_pcode( loader, reading );
   }
   struct pcode* pcode = mem_alloc( sizeof( *pcode ) );
   init_pcode( pcode, PCD_TERMINATE );
   pcode->obj_pos = 100000;
   append_pcode( reading, pcode );
}

static bool have_pcode( struct pcode_reading* reading ) {
   return ( reading->data < reading->data_end );
}

static void read_pcode( struct loader* loader, struct pcode_reading* reading ) {
   u32 obj_pos = ( u32 ) ( reading->data - loader->object_data );
   reading->obj_pos = obj_pos;
   read_opcode( loader, reading );
   switch ( reading->opcode ) {
   case PCD_GOTO:
   case PCD_IFGOTO:
   case PCD_IFNOTGOTO:
      read_jump( reading );
      break;
   case PCD_CASEGOTO:
      read_casejump( reading );
      break;
   case PCD_CASEGOTOSORTED:
      read_sortedcasejump( loader, reading );
      break;
   case PCD_PUSHBYTES:
      read_pushbytes( loader, reading );
      break;
   default:
      read_generic( loader, reading );
   }
   reading->tail->obj_pos = ( i32 ) obj_pos;
}

static void read_opcode( struct loader* loader,
   struct pcode_reading* reading ) {
   i32 opcode = PCD_NOP;
   if ( loader->small_code ) {
      u8 ch = 0;
      memcpy( &ch, reading->data, sizeof( ch ) );
      reading->data += sizeof( ch );
      opcode = ch;
      if ( opcode >= 240 ) {
         memcpy( &ch, reading->data, sizeof( ch ) );
         reading->data += sizeof( ch );
         opcode += ch;
      }
   }
   else {
      memcpy( &opcode, reading->data, sizeof( opcode ) );
      reading->data += sizeof( opcode );
   }
   reading->opcode = opcode;
}

static i32 read_uint8( struct pcode_reading* reading ) {
   u8 value;
   memcpy( &value, reading->data, sizeof( value ) );
   reading->data += sizeof( value );
   return value;
}

static i32 read_int16( struct pcode_reading* reading ) {
   i16 value;
   memcpy( &value, reading->data, sizeof( value ) );
   reading->data += sizeof( value );
   return value;
}

static i32 read_int32( struct pcode_reading* reading ) {
   i32 value;
   memcpy( &value, reading->data, sizeof( value ) );
   reading->data += sizeof( value );
   return value;
}

static void read_arg( struct pcode_reading* reading, i32* value ) {
   enum {
      SIZE_4,
      SIZE_2,
      SIZE_1
   } size = SIZE_4;
   switch ( reading->opcode ) {
   default:
      break;
   }
   switch ( size ) {
   case SIZE_1:
      {
         i8 value8;
         memcpy( &value8, reading->data, sizeof( value8 ) );
         reading->data += sizeof( value8 );
         *value = value8;
      }
      break;
   case SIZE_2:
      {
         i16 value16;
         memcpy( &value16, reading->data, sizeof( value16 ) );
         reading->data += sizeof( value16 );
         *value = value16;
      }
      break;
   default:
      {
         i32 value32;
         memcpy( &value32, reading->data, sizeof( value32 ) );
         reading->data += sizeof( value32 );
         *value = value32;
      }
   }
}

static void read_jump( struct pcode_reading* reading ) {
   struct jump_pcode* jump = mem_alloc( sizeof( *jump ) );
   init_pcode( &jump->pcode, reading->opcode );
   read_arg( reading, &jump->destination_obj_pos );
   append_pcode( reading, &jump->pcode );
}

static void read_casejump( struct pcode_reading* reading ) {
   struct casejump_pcode* jump = mem_alloc( sizeof( *jump ) );
   init_pcode( &jump->pcode, PCD_CASEGOTO );
   jump->next = NULL;
   jump->destination = NULL;
   jump->destination_obj_pos = 0;
   jump->value = 0;
   read_arg( reading, &jump->value );
   read_arg( reading, &jump->destination_obj_pos );
   append_pcode( reading, &jump->pcode );
}

static void read_sortedcasejump( struct loader* loader,
   struct pcode_reading* reading ) {
   struct sortedcasejump_pcode* jump = mem_alloc( sizeof( *jump ) );
   init_pcode( &jump->pcode, PCD_CASEGOTOSORTED );
   jump->head = NULL;
   jump->tail = NULL;
   jump->count = 0;
   reading->data = loader->object_data +
      ( ( reading->data - loader->object_data + 3 ) & ~0x3 );
   i32 count = read_int32( reading );
   for ( i32 i = 0; i < count; ++i ) {
      struct casejump_pcode* case_jump = mem_alloc( sizeof( *case_jump ) );
      init_pcode( &case_jump->pcode, PCD_CASEGOTO );
      case_jump->pcode.obj_pos = ( i32 ) reading->obj_pos;
      case_jump->next = NULL;
      case_jump->destination = NULL;
      case_jump->destination_obj_pos = 0;
      case_jump->value = 0;
      case_jump->value = read_int32( reading );
      case_jump->destination_obj_pos = read_int32( reading );
      append_casejump( jump, case_jump );
   }
   append_pcode( reading, &jump->pcode );
}

static void append_casejump( struct sortedcasejump_pcode* jump,
   struct casejump_pcode* case_jump ) {
   if ( jump->head ) {
      jump->tail->next = case_jump;
   }
   else {
      jump->head = case_jump;
   }
   jump->tail = case_jump;
   ++jump->count;
}

static void read_pushbytes( struct loader* loader,
   struct pcode_reading* reading ) {
   struct generic_pcode* generic = alloc_generic_pcode( reading->opcode,
      reading->obj_pos );
   append_pcode( reading, &generic->pcode );
   i32 count = read_uint8( reading );
   struct generic_pcode_arg* arg = alloc_generic_pcode_arg();
   arg->value = count;
   append_arg( generic, arg );
   for ( i32 i = 0; i < count; ++i ) {
      struct generic_pcode_arg* arg = alloc_generic_pcode_arg();
      arg->value = read_uint8( reading );
      append_arg( generic, arg );
   }
}

static void read_generic( struct loader* loader,
   struct pcode_reading* reading ) {
   struct generic_pcode* generic = alloc_generic_pcode( reading->opcode,
      reading->obj_pos );
   append_pcode( reading, &generic->pcode );
   struct pcode_info* pcode = c_get_pcode_info( generic->pcode.opcode );
   if ( ! pcode ) {
      t_diag( loader->task, DIAG_ERR,
         "encountered unknown pcode (opcode: %d) at position %d",
         generic->pcode.opcode, generic->pcode.obj_pos );
      t_bail( loader->task );
   }
   for ( i32 i = 0; i < pcode->argc; ++i ) {
      read_generic_arg( loader, reading, generic, i );
   }
}

static struct generic_pcode* alloc_generic_pcode( i32 opcode, u32 obj_pos ) {
   struct generic_pcode* generic = mem_alloc( sizeof( *generic ) );
   init_pcode( &generic->pcode, opcode );
   generic->pcode.obj_pos = ( i32 ) obj_pos;
   generic->args = NULL;
   generic->args_tail = NULL;
   return generic;
}

static void read_generic_arg( struct loader* loader,
   struct pcode_reading* reading, struct generic_pcode* generic,
   i32 arg_number ) {
   i32 value = 0;
   switch ( reading->opcode ) {
   // One argument instructions, with argument being 1-byte/4-bytes.
   case PCD_LSPEC1:
   case PCD_LSPEC2:
   case PCD_LSPEC3:
   case PCD_LSPEC4:
   case PCD_LSPEC5:
   case PCD_LSPEC5RESULT:
   case PCD_ASSIGNSCRIPTVAR:
   case PCD_ASSIGNMAPVAR:
   case PCD_ASSIGNWORLDVAR:
   case PCD_PUSHSCRIPTVAR:
   case PCD_PUSHMAPVAR:
   case PCD_PUSHWORLDVAR:
   case PCD_ADDSCRIPTVAR:
   case PCD_ADDMAPVAR:
   case PCD_ADDWORLDVAR:
   case PCD_SUBSCRIPTVAR:
   case PCD_SUBMAPVAR:
   case PCD_SUBWORLDVAR:
   case PCD_MULSCRIPTVAR:
   case PCD_MULMAPVAR:
   case PCD_MULWORLDVAR:
   case PCD_DIVSCRIPTVAR:
   case PCD_DIVMAPVAR:
   case PCD_DIVWORLDVAR:
   case PCD_MODSCRIPTVAR:
   case PCD_MODMAPVAR:
   case PCD_MODWORLDVAR:
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
   case PCD_ASSIGNGLOBALVAR:
   case PCD_PUSHGLOBALVAR:
   case PCD_ADDGLOBALVAR:
   case PCD_SUBGLOBALVAR:
   case PCD_MULGLOBALVAR:
   case PCD_DIVGLOBALVAR:
   case PCD_MODGLOBALVAR:
   case PCD_INCGLOBALVAR:
   case PCD_DECGLOBALVAR:
   case PCD_CALL:
   case PCD_CALLDISCARD:
   case PCD_PUSHMAPARRAY:
   case PCD_ASSIGNMAPARRAY:
   case PCD_ADDMAPARRAY:
   case PCD_SUBMAPARRAY:
   case PCD_MULMAPARRAY:
   case PCD_DIVMAPARRAY:
   case PCD_MODMAPARRAY:
   case PCD_INCMAPARRAY:
   case PCD_DECMAPARRAY:
   case PCD_PUSHWORLDARRAY:
   case PCD_ASSIGNWORLDARRAY:
   case PCD_ADDWORLDARRAY:
   case PCD_SUBWORLDARRAY:
   case PCD_MULWORLDARRAY:
   case PCD_DIVWORLDARRAY:
   case PCD_MODWORLDARRAY:
   case PCD_INCWORLDARRAY:
   case PCD_DECWORLDARRAY:
   case PCD_PUSHGLOBALARRAY:
   case PCD_ASSIGNGLOBALARRAY:
   case PCD_ADDGLOBALARRAY:
   case PCD_SUBGLOBALARRAY:
   case PCD_MULGLOBALARRAY:
   case PCD_DIVGLOBALARRAY:
   case PCD_MODGLOBALARRAY:
   case PCD_INCGLOBALARRAY:
   case PCD_DECGLOBALARRAY:
   case PCD_ANDSCRIPTVAR:
   case PCD_ANDMAPVAR:
   case PCD_ANDGLOBALVAR:
   case PCD_ANDMAPARRAY:
   case PCD_ANDWORLDARRAY:
   case PCD_ANDGLOBALARRAY:
   case PCD_EORSCRIPTVAR:
   case PCD_EORMAPVAR:
   case PCD_EORWORLDVAR:
   case PCD_EORGLOBALVAR:
   case PCD_EORMAPARRAY:
   case PCD_EORWORLDARRAY:
   case PCD_EORGLOBALARRAY:
   case PCD_ORSCRIPTVAR:
   case PCD_ORMAPVAR:
   case PCD_ORWORLDVAR:
   case PCD_ORGLOBALVAR:
   case PCD_ORMAPARRAY:
   case PCD_ORWORLDARRAY:
   case PCD_ORGLOBALARRAY:
   case PCD_LSSCRIPTVAR:
   case PCD_LSMAPVAR:
   case PCD_LSWORLDVAR:
   case PCD_LSGLOBALVAR:
   case PCD_LSMAPARRAY:
   case PCD_LSWORLDARRAY:
   case PCD_LSGLOBALARRAY:
   case PCD_RSSCRIPTVAR:
   case PCD_RSMAPVAR:
   case PCD_RSWORLDVAR:
   case PCD_RSGLOBALVAR:
   case PCD_RSMAPARRAY:
   case PCD_RSWORLDARRAY:
   case PCD_RSGLOBALARRAY:
   case PCD_PUSHFUNCTION:
   case PCD_ASSIGNSCRIPTARRAY:
   case PCD_PUSHSCRIPTARRAY:
   case PCD_ADDSCRIPTARRAY:
   case PCD_SUBSCRIPTARRAY:
   case PCD_MULSCRIPTARRAY:
   case PCD_DIVSCRIPTARRAY:
   case PCD_MODSCRIPTARRAY:
   case PCD_INCSCRIPTARRAY:
   case PCD_DECSCRIPTARRAY:
   case PCD_ANDSCRIPTARRAY:
   case PCD_EORSCRIPTARRAY:
   case PCD_ORSCRIPTARRAY:
   case PCD_LSSCRIPTARRAY:
   case PCD_RSSCRIPTARRAY:
      if ( loader->small_code ) {
         value = read_uint8( reading );
      }
      else {
         value = read_int32( reading );
      }
      break;
   case PCD_LSPEC1DIRECT:
   case PCD_LSPEC2DIRECT:
   case PCD_LSPEC3DIRECT:
   case PCD_LSPEC4DIRECT:
   case PCD_LSPEC5DIRECT:
      if ( loader->small_code && arg_number == 0 ) {
         value = read_uint8( reading );
      }
      else {
         value = read_int32( reading );
      }
      break;
   case PCD_CALLFUNC:
      if ( loader->small_code ) {
         // Argument-count field.
         if ( arg_number == 0 ) {
            value = read_uint8( reading );
         }
         // Function-index field.
         else {
            value = read_int16( reading );
         }
      }
      else {
         value = read_int32( reading );
      }
      break;
   case PCD_PUSHNUMBER:
   case PCD_THINGCOUNTDIRECT:
   case PCD_SCRIPTWAITDIRECT:
   case PCD_CONSOLECOMMANDDIRECT:
   case PCD_LSPEC5EX:
   case PCD_LSPEC5EXRESULT:
      value = read_int32( reading );
      break;
   case PCD_PUSHBYTE:
   case PCD_PUSH2BYTES:
   case PCD_PUSH3BYTES:
   case PCD_PUSH4BYTES:
   case PCD_PUSH5BYTES:
   case PCD_LSPEC1DIRECTB:
   case PCD_LSPEC2DIRECTB:
   case PCD_LSPEC3DIRECTB:
   case PCD_LSPEC4DIRECTB:
   case PCD_LSPEC5DIRECTB:
   case PCD_DELAYDIRECTB:
   case PCD_RANDOMDIRECTB:
      value = read_uint8( reading );
      break;
   default:
      t_diag( loader->task, DIAG_INTERNAL | DIAG_ERR,
         "unhandled pcode (%d) at: %s:%d", reading->opcode, __FILE__,
         __LINE__ );
      t_bail( loader->task );
   }
   struct generic_pcode_arg* arg = mem_alloc( sizeof( *arg ) );
   arg->next = NULL;
   arg->value = value;
   append_arg( generic, arg );
}

static struct generic_pcode_arg* alloc_generic_pcode_arg( void ) {
   struct generic_pcode_arg* arg = mem_alloc( sizeof( *arg ) );
   arg->next = NULL;
   arg->value = 0;
   return arg;
}

static void append_arg( struct generic_pcode* generic,
   struct generic_pcode_arg* arg ) {
   if ( generic->args ) {
      generic->args_tail->next = arg;
   }
   else {
      generic->args = arg;
   }
   generic->args_tail = arg;
}

static void init_pcode( struct pcode* pcode, i32 opcode ) {
   pcode->prev = NULL;
   pcode->next = NULL;
   pcode->note = NULL;
   pcode->opcode = opcode;
   pcode->obj_pos = 0;
}

static void patch_script_jumps( struct task* task ) {
   struct list_iter i;
   list_iterate( &task->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      //struct jump_patch patch;
      //init_jump_patch( &patch, script->pcode );
      //patch_jumps( task, &patch );
      list_next( &i );
   }
}

// For each script and function, this function connects the jump pcodes to
// their destination pcodes.
static void patch( struct task* task ) {
   patch_scripts( task );
   patch_funcs( task );
}

static void patch_scripts( struct task* task ) {
   struct list_iter i;
   list_iterate( &task->scripts, &i );
   while ( ! list_end( &i ) ) {
      struct script* script = list_data( &i );
      struct patch patch;
      init_patch( &patch, script->body_start, script->body_end );
      patch_script( &patch );
      list_next( &i );
   }
}

static void patch_funcs( struct task* task ) {
   struct list_iter i;
   list_iterate( &task->funcs, &i );
   while ( ! list_end( &i ) ) {
      struct func* func = list_data( &i );
      struct patch patch;
      init_patch( &patch, func->more.user->start, func->more.user->end );
      patch_script( &patch );
      list_next( &i );
   }
}

static void init_patch( struct patch* patch, struct pcode* begin,
   struct pcode* end ) {
   patch->begin = begin;
   patch->end = end;
}

static void patch_script( struct patch* patch ) {
   struct pcode* pcode = patch->begin;
   while ( pcode != patch->end->next ) {
      switch ( pcode->opcode ) {
      case PCD_GOTO:
      case PCD_IFGOTO:
      case PCD_IFNOTGOTO:
         patch_jump( patch,
            ( struct jump_pcode* ) pcode );
         break;
      case PCD_CASEGOTO:
         patch_casejump( patch,
            ( struct casejump_pcode* ) pcode );
         break;
      case PCD_CASEGOTOSORTED:
         patch_sortedcasejump( patch,
            ( struct sortedcasejump_pcode* ) pcode );
         break;
      default:
         break;
      }
      pcode = pcode->next;
   }
}

static void patch_jump( struct patch* patch, struct jump_pcode* jump ) {
   jump->destination = find_destination( patch, &jump->pcode,
      jump->destination_obj_pos );
}

static void patch_casejump( struct patch* patch,
   struct casejump_pcode* jump ) {
   jump->destination = find_destination( patch, &jump->pcode,
      jump->destination_obj_pos );
}

static void patch_sortedcasejump( struct patch* patch,
   struct sortedcasejump_pcode* jump ) {
   struct casejump_pcode* case_jump = jump->head;
   while ( case_jump ) {
      patch_casejump( patch, case_jump );
      case_jump = case_jump->next;
   }
}

static struct pcode* find_destination( struct patch* patch,
   struct pcode* jump, i32 obj_pos ) {
   struct pcode* pcode = patch->begin;
   struct pcode* end = jump;
   if ( obj_pos > end->obj_pos ) {
      pcode = end->next;
      end = patch->end->next;
   }
   while ( pcode != end && pcode->obj_pos != obj_pos ) {
      pcode = pcode->next;
   }
   return pcode;
}

void t_show( struct task* task ) {
   struct list_iter i;
   list_iterate( &task->scripts, &i );
   while ( ! list_end( &i ) ) {
      show_script( task, list_data( &i ) );
      list_next( &i );
   }
}

static void show_script( struct task* task, struct script* script ) {
   struct pcode* pcode = script->body_start;
   while ( pcode ) {
      printf( "> %d\n", pcode->opcode );
      pcode = pcode->next;
   }
}

static void diag( struct loader* loader, u32 flags, ... ) {
   va_list args;
   va_start( args, flags );
   t_diag_args( loader->task, ( i32 ) flags, &args );
   va_end( args );
}

static void bail( struct loader* loader ) {
   t_bail( loader->task );
}
