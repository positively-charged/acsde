/**
 * Entry point.
 */

#include <stdio.h>
#include <string.h>

#include "task.h"

struct diag_msg {
   struct str text;
   i32 flags;
};

static void init_options( struct options* options );
static void read_options( struct options* options, char* argv[] );
static void print_usage( char* path );
static bool disassemble( struct options* options );
static bool decompile( struct options* options );
static void init_task( struct task* task, struct options* options );
static void init_diag_msg( struct task* task, struct diag_msg* msg, i32 flags,
   va_list* args );
static void print_diag( struct task* task, struct diag_msg* msg );

i32 main( i32 argc, char* argv[] ) {
   mem_init();
   struct options options;
   init_options( &options );
   read_options( &options, argv );
   i32 result = EXIT_FAILURE;
   if ( options.object_file ) {
      if ( options.disassemble ) {
         bool disassembled = disassemble( &options );
         if ( disassembled ) {
            result = EXIT_SUCCESS;
         }
      }
      else {
         bool decompiled = decompile( &options );
         if ( decompiled ) {
            result = EXIT_SUCCESS;
         }
      }
   }
   else {
      print_usage( argv[ 0 ] );
   }
   mem_free_all();
   return result;
}

static void init_options( struct options* options ) {
   options->object_file = NULL;
   options->source_file = NULL;
   options->disassemble = false;
}

static void read_options( struct options* options, char* argv[] ) {
   char** args = argv + 1;
   while ( true ) {
      // Select option.
      const char* option = *args;
      if ( option && *option == '-' ) {
         ++option;
         ++args;
      }
      else {
         break;
      }
      // Process option.
      if ( strcmp( option, "a" ) == 0 ) {
         options->disassemble = true;
      }
      else {
         printf( "error: unknown option: %s\n", option );
         return;
      }
   }
   if ( *args ) {
      options->object_file = *args;
      ++args;
      if ( *args ) {
         options->source_file = *args;
      }
   }
}

static void print_usage( char* path ) {
   printf(
      "Usage: %s [options] <object-file> [output-file]\n"
      "Options:\n"
      "  -a    Disassemble\n"
      "",
      path );
}

static bool disassemble( struct options* options ) {
   bool disassembled = false;
   struct task task;
   init_task( &task, options );
   if ( setjmp( task.bail ) == 0 ) {
      t_load( &task );
      t_show( &task );
      disassembled = true;
   }
   return disassembled;
}

static bool decompile( struct options* options ) {
   bool decompiled = false;
   struct task task;
   init_task( &task, options );
   if ( setjmp( task.bail ) == 0 ) {
      t_create_builtins( &task );
      t_load( &task );
      t_annotate( &task );
      t_recover( &task );
      t_analyze( &task );
      t_publish( &task );
      decompiled = true;
   }
   return decompiled;
}

static void init_task( struct task* task, struct options* options ) {
   task->options = options;
   str_init( &task->library_name );
   list_init( &task->objects );
   list_init( &task->scripts );
   list_init( &task->funcs );
   list_init( &task->imports );
   for ( u32 i = 0; i < ARRAY_SIZE( task->map_vars ); ++i ) {
      task->map_vars[ i ] = NULL;
   }
   for ( u32 i = 0; i < ARRAY_SIZE( task->world_vars ); ++i ) {
      task->world_vars[ i ] = NULL;
      task->world_arrays[ i ] = NULL;
   }
   for ( u32 i = 0; i < ARRAY_SIZE( task->global_vars ); ++i ) {
      task->global_vars[ i ] = NULL;
      task->global_arrays[ i ] = NULL;
   }
   task->strings = NULL;
   task->num_strings = 0u;
   task->encrypt_str = false;
   task->importable = false;
   task->compact = false;
   task->wadauthor = false;
   task->calls_aspec = false;
   task->calls_ext = false;
}

bool t_uses_zcommon_file( struct task* task ) {
   return ( task->calls_aspec || task->calls_ext );
}

void t_bail( struct task* task ) {
   longjmp( task->bail, 1 );
}

void t_diag( struct task* task, i32 flags, ... ) {
   va_list args;
   va_start( args, flags );
   struct diag_msg msg;
   init_diag_msg( task, &msg, flags, &args );
   print_diag( task, &msg );
   va_end( args );
}

void t_diag_args( struct task* task, i32 flags, va_list* args ) {
   struct diag_msg msg;
   init_diag_msg( task, &msg, flags, args );
   print_diag( task, &msg );
}

static void init_diag_msg( struct task* task, struct diag_msg* msg, i32 flags,
   va_list* args ) {
   str_init( &msg->text );
   msg->flags = flags;
   // Append message prefix.
   if ( flags & DIAG_INTERNAL ) {
      str_append( &msg->text, "internal " );
   }
   if ( flags & DIAG_ERR ) {
      str_append( &msg->text, "error: " );
   }
   else if ( flags & DIAG_WARN ) {
      str_append( &msg->text, "warning: " );
   }
   else if ( flags & DIAG_NOTE ) {
      str_append( &msg->text, "note: " );
   }
   // Append message.
   const char* format = va_arg( *args, const char* );
   str_append_format( &msg->text, format, args );
}

static void print_diag( struct task* task, struct diag_msg* msg ) {
   printf( "%s\n", msg->text.value );
}

struct func* t_alloc_func( void ) {
   struct func* func = mem_slot_alloc( sizeof( *func ) );
   func->node.type = NODE_FUNC;
   func->type = FUNC_ASPEC;
   str_init( &func->name );
   list_init( &func->params );
   func->return_spec = SPEC_VOID;
   func->min_param = 0;
   func->max_param = 0;
   return func;
}

struct param* t_alloc_param( void ) {
   struct param* param = mem_slot_alloc( sizeof( *param ) );
   param->spec = SPEC_NONE;
   return param;
}

const char* t_lookup_string( struct task* task, u32 index ) {
   if ( index < task->num_strings ) {
      return task->strings[ index ].value;
   }
   else {
      return NULL;
   }
}
