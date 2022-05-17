/*

   Stage 5 -- Code Generation

   In this stage, we output ACS code.

*/

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "task.h"

struct codegen {
   struct task* task;
   i32 indent_level;
   bool got_newline;
   FILE* output;
};

static void init_codegen( struct codegen* codegen, struct task* task );
static void open_output_file( struct codegen* codegen );
static void write( struct codegen* codegen, const char* format, ... );
static void emit( struct codegen* codegen );
static void write_dircs( struct codegen* codegen );
static void write_global_vars( struct codegen* codegen );
static void write_world_vars( struct codegen* codegen );
static void visit_var_dec( struct codegen* codegen, struct var* var );
static void write_value( struct codegen* codegen, i32 type, i32 value );
static void write_map_vars( struct codegen* codegen );
static void write_objects( struct codegen* codegen );
static void show_script( struct codegen* codegen, struct script* script );
static void write_func( struct codegen* codegen, struct func* func );
static void emit_block( struct codegen* codegen, struct block* block );
static void visit_stmt( struct codegen* codegen, struct node* node );
static void visit_var( struct codegen* codegen, struct var* var );
static void emit_if( struct codegen* codegen, struct if_stmt* stmt );
static void emit_cond( struct codegen* codegen, struct expr* expr );
static void emit_switch( struct codegen* codegen, struct switch_stmt* stmt );
static void emit_case( struct codegen* codegen, struct case_label* label );
static void emit_default_case( struct codegen* codegen,
   struct case_label* label );
static void emit_while( struct codegen* codegen, struct while_stmt* stmt );
static void emit_do( struct codegen* codegen, struct do_stmt* stmt );
static void visit_for( struct codegen* codegen, struct for_stmt* stmt );
static void emit_jump( struct codegen* codegen, struct jump* jump );
static void show_script_jump( struct codegen* codegen,
   struct script_jump* jump );
static void visit_return( struct codegen* codegen, struct return_stmt* stmt );
static void emit_inline_asm( struct codegen* codegen,
   struct inline_asm* inline_asm );
static void show_expr_stmt( struct codegen* codegen, struct expr_stmt* stmt );
static void emit_expr( struct codegen* codegen, struct expr* expr );
static void emit_operand( struct codegen* codegen, struct node* node );
static void emit_binary( struct codegen* codegen, struct binary* binary );
static void emit_assign( struct codegen* codegen, struct assign* assign );
static void visit_prefix( struct codegen* codegen, struct node* node );
static void emit_binary( struct codegen* codegen, struct binary* binary );
static void emit_unary( struct codegen* codegen, struct unary* unary );
static void visit_inc( struct codegen* codegen, struct inc* inc );
static void visit_suffix( struct codegen* codegen, struct node* node );
static void visit_post_inc( struct codegen* codegen, struct inc* inc );
static void visit_subscript( struct codegen* codegen,
   struct subscript* subscript );
static void emit_call( struct codegen* codegen, struct call* call );
static void write_format_item_list( struct codegen* codegen,
   struct call* call );
static void write_format_item_array( struct codegen* codegen,
   struct call* call, struct format_item* item );
static void visit_primary( struct codegen* codegen, struct node* node );
static void emit_literal( struct codegen* codegen, struct literal* literal );
static void visit_name_usage( struct codegen* codegen,
   struct name_usage* usage );
static void emit_aspec( struct codegen* codegen, struct aspec* aspec );
static void emit_func( struct codegen* codegen, struct func* func );
static void emit_var( struct codegen* codegen, struct var* var );
static void write_var_name( struct codegen* codegen, struct var* var );
static void visit_strcpy_call( struct codegen* codegen,
   struct strcpy_call* call );
static void visit_paren( struct codegen* codegen, struct paren* paren );
static void visit_unknown( struct codegen* codegen, struct unknown* unknown );
static void visit_paltrans( struct codegen* codegen, struct paltrans* trans );

void t_publish( struct task* task ) {
   struct codegen codegen;
   init_codegen( &codegen, task );
   open_output_file( &codegen );
   emit( &codegen );
   //close_output_file( &codegen );
}

static void init_codegen( struct codegen* codegen, struct task* task ) {
   codegen->task = task;
   codegen->indent_level = 0;
   codegen->got_newline = false;
   codegen->output = stdout;
}

static void open_output_file( struct codegen* codegen ) {
   if ( codegen->task->options->source_file ) {
      FILE* fh = fopen( codegen->task->options->source_file, "w" );
      if ( ! fh ) {
         printf( "error: failed to open output file\n" );
         exit( EXIT_FAILURE );
      }
      codegen->output = fh;
   }
   //printf( "%s\n", codegen->task->options->source_file );
}

void write( struct codegen* codegen, const char* format, ... ) {
   if ( codegen->got_newline ) {
      enum { INDENT_WIDTH = 3 }; // Amount of spaces.
      i32 num_spaces = codegen->indent_level * INDENT_WIDTH;
      for ( i32 i = 0; i < num_spaces; ++i ) {
         fprintf( codegen->output, " " );
      }
   }
   va_list args;
   va_start( args, format );
   vfprintf( codegen->output, format, args );
   va_end( args );
   codegen->got_newline = false;
}

void write_nl( struct codegen* codegen ) {
   fprintf( codegen->output, "\n" );
   codegen->got_newline = true;
}

void indent( struct codegen* codegen ) {
   ++codegen->indent_level;
}

void dedent( struct codegen* codegen ) {
   --codegen->indent_level;
}

static void emit( struct codegen* codegen ) {
   write_dircs( codegen );
   write_global_vars( codegen );
   write_world_vars( codegen );
   write_map_vars( codegen );
   write_objects( codegen );
}

static void write_dircs( struct codegen* codegen ) {
   if ( codegen->task->importable ) {
      write( codegen, "#library \"%s\"", codegen->task->library_name.value );
      write_nl( codegen );
      write_nl( codegen );
   }
   if ( ! codegen->task->compact ) {
      write( codegen, "#nocompact" );
      write_nl( codegen );
   }
   if ( ! codegen->task->wadauthor ) {
      // A library already has #nowadauthor implicitly enabled, so there is no
      // need to explicitly specify it. There is also no need to specify the
      // directive when there are no scripts.
      if ( ! codegen->task->importable &&
         list_size( &codegen->task->scripts ) > 0 ) {
         write( codegen, "#nowadauthor" );
         write_nl( codegen );
      }
   }
   if ( codegen->task->encrypt_str ) {
      write( codegen, "#encryptstrings" );
      write_nl( codegen );
      write_nl( codegen );
   }
   if ( t_uses_zcommon_file( codegen->task ) ) {
      write( codegen, "#include \"zcommon.acs\"" );
      write_nl( codegen );
      write_nl( codegen );
   }

   struct list_iter i;
   list_iterate( &codegen->task->imports, &i );
   while ( ! list_end( &i ) ) {
      struct imported_module* module = list_data( &i );
      write( codegen, "#import \"%s.acs\"", module->name.value );
      write_nl( codegen );
      list_next( &i );
   }
}

static void write_global_vars( struct codegen* codegen ) {
   u32 i = 0;
   while ( i < ARRAY_SIZE( codegen->task->global_vars ) &&
      ! codegen->task->global_vars[ i ] ) {
      ++i;
   }
   if ( i < ARRAY_SIZE( codegen->task->global_vars ) ) {
      while ( i < ARRAY_SIZE( codegen->task->global_vars ) ) {
         if ( codegen->task->global_vars[ i ] ) {
            visit_var_dec( codegen, codegen->task->global_vars[ i ] );
         }
         ++i;
      }
   }

   i = 0;
   while ( i < ARRAY_SIZE( codegen->task->global_arrays ) ) {
      if ( codegen->task->global_arrays[ i ] ) {
         visit_var_dec( codegen, codegen->task->global_arrays[ i ] );
      }
      ++i;
   }
   write_nl( codegen );
}

static void write_world_vars( struct codegen* codegen ) {
   u32 i = 0;
   while ( i < ARRAY_SIZE( codegen->task->world_vars ) &&
      ! codegen->task->world_vars[ i ] ) {
      ++i;
   }
   if ( i < ARRAY_SIZE( codegen->task->world_vars ) ) {
      while ( i < ARRAY_SIZE( codegen->task->world_vars ) ) {
         if ( codegen->task->world_vars[ i ] ) {
            visit_var_dec( codegen, codegen->task->world_vars[ i ] );
         }
         ++i;
      }
   }

   i = 0;
   while ( i < ARRAY_SIZE( codegen->task->world_arrays ) ) {
      if ( codegen->task->world_arrays[ i ] ) {
         visit_var_dec( codegen, codegen->task->world_arrays[ i ] );
      }
      ++i;
   }
   write_nl( codegen );
}

static void visit_var_dec( struct codegen* codegen, struct var* var ) {
   if ( var->imported ) {
      write( codegen, "// " );
   }
   // Storage.
   switch ( var->storage ) {
   case STORAGE_WORLD:
      write( codegen, "world " );
      break;
   case STORAGE_GLOBAL:
      write( codegen, "global " );
      break;
   default:
      break;
   }
   // Type.
   switch ( var->spec ) {
   case SPEC_INT:
   default:
      write( codegen, "int" );
      break;
   case SPEC_STR:
      write( codegen, "str" );
      break;
   }
   write( codegen, " " );
   // Storage index.
   switch ( var->storage ) {
   case STORAGE_WORLD:
   case STORAGE_GLOBAL:
      write( codegen, "%d:", var->index );
      break;
   case STORAGE_LOCAL:
   case STORAGE_MAP:
      break;
   }
   // Name.
   write_var_name( codegen, var );
   // Dimension.
   if ( var->array ) {
      if ( var->dim_length > 0 ) {
         write( codegen, "[ %d ]", var->dim_length );
      }
      else {
         write( codegen, "[]" );
      }
   }
   // Initializer.
   if ( var->initz ) {
      write( codegen, " = " );
      emit_expr( codegen, var->initz );
   }
   else if ( var->value ) {
      write( codegen, " = { " );
      struct value* value = var->value;
      i32 index = 0;
      while ( value ) {
         while ( index < value->index ) {
            write_value( codegen, value->type, 0 );
            write( codegen, ", " );
            ++index;
         }
         write_value( codegen, value->type, value->value );
         ++index;
         value = value->next;
         if ( value ) {
            write( codegen, ", " );
         }
      }
      write( codegen, " }" );
   }
   // End.
   write( codegen, ";" );
   write_nl( codegen );
}

static void write_value( struct codegen* codegen, i32 type, i32 value ) {
   if ( type == VALUE_STRING ) {
      const char* string = t_lookup_string( codegen->task, ( u32 ) value );
      if ( string ) {
         write( codegen, "\"%s\"", string );
      }
      else {
         write( codegen, "%d", value );
      }
   }
   else {
      write( codegen, "%d", value );
   }
}

static void write_map_vars( struct codegen* codegen ) {
   u32 i = 0;
   while ( i < ARRAY_SIZE( codegen->task->map_vars ) &&
      ! codegen->task->map_vars[ i ] ) {
      ++i;
   }
   if ( i < ARRAY_SIZE( codegen->task->map_vars ) ) {
      while ( i < ARRAY_SIZE( codegen->task->map_vars ) ) {
         if ( codegen->task->map_vars[ i ] ) {
            visit_var_dec( codegen, codegen->task->map_vars[ i ] );
         }
         ++i;
      }
      write_nl( codegen );
   }
}

static void write_objects( struct codegen* codegen ) {
   struct list_iter i;
   list_iterate( &codegen->task->objects, &i );
   while ( ! list_end( &i ) ) {
      struct node* node = list_data( &i );
      switch ( node->type ) {
      case NODE_SCRIPT:
         show_script( codegen,
            ( struct script* ) node );
         break;
      case NODE_FUNC:
         write_func( codegen,
            ( struct func* ) node );
         break;
      default:
         UNREACHABLE();
         t_bail( codegen->task );
      }
      list_next( &i );
      if ( ! list_end( &i ) ) {
         write_nl( codegen );
      }
   }
}

static void show_script( struct codegen* codegen, struct script* script ) {
   write( codegen, "script " );
   if ( script->named_script ) {
      write( codegen, "\"%s\" ", script->name.value );
   }
   else {
      write( codegen, "%d ", script->number );
   }
   if ( script->num_param > 0 ) {
      write( codegen, "( " );
      for ( u32 i = 0; i < script->num_param; ++i ) {
         write( codegen, "int " );
         write_var_name( codegen, script->vars[ i ] );
         if ( i < script->num_param - 1 ) {
            write( codegen, ", " );
         }
      }
      write( codegen, " ) " );
   }
   else {
      if ( script->type == SCRIPT_TYPE_CLOSED ) {
         write( codegen, "( void ) " );
      }
   }
   // Script type.
   const char* name = NULL;
   switch ( script->type ) {
   case SCRIPT_TYPE_OPEN:
      name = "open";
      break;
   case SCRIPT_TYPE_ENTER:
      name = "enter";
      break;
   case SCRIPT_TYPE_DEATH:
      name = "death";
      break;
   case SCRIPT_TYPE_DISCONNECT:
      name = "disconnect";
      break;
   case SCRIPT_TYPE_EVENT:
      name = "event";
      break;
   default:
      break;
   }
   if ( name ) {
      write( codegen, "%s ", name );
   }
   // Script flags.
   if ( script->flags & SCRIPTFLAG_NET ) {
      write( codegen, "net " );
   }
   if ( script->flags & SCRIPTFLAG_CLIENTSIDE ) {
      write( codegen, "clientside " );
   }
   // Body.
   emit_block( codegen, script->body );
}

static void write_func( struct codegen* codegen, struct func* func ) {
   write( codegen, "function" );
   // Return type.
   const char* name = "void";
   switch ( func->return_spec ) {
   case SPEC_INT: name = "int"; break;
   default: break;
   }
   write( codegen, " %s", name );
   // Function name.
   write( codegen, " " );
   if ( func->name.length > 0 ) {
      write( codegen, func->name.value );
   }
   else {
      write( codegen, "Func%d", func->more.user->index );
   }
   // Parameter list.
   write( codegen, "( " );
   if ( func->max_param > 0 ) {
      for ( i32 i = 0; i < func->max_param; ++i ) {
         write( codegen, "int " );
         write( codegen, "var%d", i );
         if ( i < func->max_param - 1 ) {
            write( codegen, ", " );
         }
      }
   }
   else {
      write( codegen, "void" );
   }
   write( codegen, " )" );
   // Body.
   write( codegen, " " );
   emit_block( codegen, func->more.user->body );
}

static void emit_block( struct codegen* codegen, struct block* block ) {
   write( codegen, "{" );
   indent( codegen );
   if ( list_size( &block->stmts ) > 0 ) {
      write_nl( codegen );
      struct list_iter i;
      list_iterate( &block->stmts, &i );
      while ( ! list_end( &i ) ) {
         struct node* node = list_data( &i );
         visit_stmt( codegen, node );
         list_next( &i );
      }
   }
   dedent( codegen );
   write( codegen, "}" );
   write_nl( codegen );
}

static void visit_stmt( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_VAR:
      visit_var( codegen,
         ( struct var* ) node );
      break;
   case NODE_IF:
      emit_if( codegen,
         ( struct if_stmt* ) node );
      break;
   case NODE_SWITCH:
      emit_switch( codegen,
         ( struct switch_stmt* ) node );
      break;
   case NODE_CASE:
      emit_case( codegen,
         ( struct case_label* ) node );
      break;
   case NODE_CASEDEFAULT:
      emit_default_case( codegen,
         ( struct case_label* ) node );
      break;
   case NODE_WHILE:
      emit_while( codegen,
         ( struct while_stmt* ) node );
      break;
   case NODE_DO:
      emit_do( codegen,
         ( struct do_stmt* ) node );
      break;
   case NODE_FOR:
      visit_for( codegen,
         ( struct for_stmt* ) node );
      break;
   case NODE_JUMP:
      emit_jump( codegen,
         ( struct jump* ) node );
      break;
   case NODE_SCRIPTJUMP:
      show_script_jump( codegen,
         ( struct script_jump* ) node );
      break;
   case NODE_RETURN:
      visit_return( codegen,
         ( struct return_stmt* ) node );
      break;
   case NODE_INLINEASM:
      emit_inline_asm( codegen,
         ( struct inline_asm* ) node );
      break;
   case NODE_EXPRSTMT:
      show_expr_stmt( codegen,
         ( struct expr_stmt* ) node );
      break;
   default:
      UNREACHABLE();
      t_bail( codegen->task );
   }
}

static void visit_var( struct codegen* codegen, struct var* var ) {
   visit_var_dec( codegen, var );
}

static void emit_if( struct codegen* codegen, struct if_stmt* stmt ) {
   write( codegen, "if " );
   emit_cond( codegen, stmt->cond );
   emit_block( codegen, stmt->body );
   if ( stmt->else_body ) {
      struct if_stmt* else_if = NULL;
      if ( list_size( &stmt->else_body->stmts ) == 1 ) {
         struct node* node = list_head( &stmt->else_body->stmts );
         if ( node->type == NODE_IF ) {
            else_if = ( struct if_stmt* ) node;
         }
      }
      write( codegen, "else " );
      if ( else_if ) {
         emit_if( codegen, else_if );
      }
      else {
         emit_block( codegen, stmt->else_body );
      }
   }
}

static void emit_cond( struct codegen* codegen, struct expr* expr ) {
   write( codegen, "( " );
   emit_expr( codegen, expr );
   write( codegen, " ) ");
}

static void emit_switch( struct codegen* codegen, struct switch_stmt* stmt ) {
   write( codegen, "switch " );
   emit_cond( codegen, stmt->cond );
   emit_block( codegen, stmt->body );
}

static void emit_case( struct codegen* codegen, struct case_label* label ) {
   dedent( codegen );
   write( codegen, "case %d:", label->value );
   write_nl( codegen );
   indent( codegen );
}

static void emit_default_case( struct codegen* codegen,
   struct case_label* label ) {
   dedent( codegen );
   write( codegen, "default:" );
   write_nl( codegen );
   indent( codegen );
}

static void emit_while( struct codegen* codegen, struct while_stmt* stmt ) {
   write( codegen, "%s ", stmt->until ? "until" : "while" );
   emit_cond( codegen, stmt->cond );
   emit_block( codegen, stmt->body );
}

static void emit_do( struct codegen* codegen, struct do_stmt* stmt ) {
   write( codegen, "do " );
   emit_block( codegen, stmt->body );
   write( codegen, "%s ", stmt->until ? "until" : "while" );
   emit_cond( codegen, stmt->cond );
   write( codegen, ";" );
   write_nl( codegen );
}

static void visit_for( struct codegen* codegen, struct for_stmt* stmt ) {
   write( codegen, "for ( ; " );
   emit_expr( codegen, stmt->cond );
   write( codegen, "; " );
   struct list_iter i;
   list_iterate( &stmt->post, &i );
   while ( ! list_end( &i ) ) {
      emit_expr( codegen, list_data( &i ) );
      list_next( &i );
      if ( ! list_end( &i ) ) {
         write( codegen, ", " );
      }
   }
   write( codegen, " ) " );
   emit_block( codegen, stmt->body );
}

static void emit_jump( struct codegen* codegen, struct jump* jump ) {
   switch ( jump->type ) {
   case JUMP_BREAK:
      write( codegen, "break;" );
      break;
   case JUMP_CONTINUE:
      write( codegen, "continue;" );
      break;
   default:
      break;
   }
   write_nl( codegen );
}

static void show_script_jump( struct codegen* codegen,
   struct script_jump* jump ) {
   switch ( jump->type ) {
   case SCRIPTJUMP_RESTART:
      write( codegen, "restart;" );
      break;
   case SCRIPTJUMP_SUSPEND:
      write( codegen, "suspend;" );
      break;
   default:
      write( codegen, "terminate;" );
   }
   write_nl( codegen );
}

static void visit_return( struct codegen* codegen, struct return_stmt* stmt ) {
   write( codegen, "return" );
   if ( stmt->return_value ) {
      write( codegen, " " );
      emit_expr( codegen, stmt->return_value );
   }
   write( codegen, ";" );
   write_nl( codegen );
}

static void emit_inline_asm( struct codegen* codegen,
   struct inline_asm* inline_asm ) {
   write( codegen, "// > %d", inline_asm->pcode->opcode );
   write_nl( codegen );
}

static void show_expr_stmt( struct codegen* codegen, struct expr_stmt* stmt ) {
   emit_operand( codegen, stmt->expr->root );
   write( codegen, ";" );
   write_nl( codegen );
}

static void emit_expr( struct codegen* codegen, struct expr* expr ) {
   emit_operand( codegen, expr->root );
}

static void emit_operand( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_BINARY:
      emit_binary( codegen,
         ( struct binary* ) node );
      break;
   case NODE_ASSIGN:
      emit_assign( codegen,
         ( struct assign* ) node );
      break;
   default:
      visit_prefix( codegen,
         node );
   }
}

static void emit_binary( struct codegen* codegen, struct binary* binary ) {
   emit_operand( codegen, binary->lside );
   STATIC_ASSERT( BOP_TOTAL == 19 );
   const char* text = "";
   switch ( binary->op ) {
   case BOP_LOG_OR: text = "||"; break;
   case BOP_LOG_AND: text = "&&"; break;
   case BOP_BIT_OR: text = "|"; break;
   case BOP_BIT_XOR: text = "^"; break;
   case BOP_BIT_AND: text = "&"; break;
   case BOP_EQ: text = "=="; break;
   case BOP_NEQ: text = "!="; break;
   case BOP_LT: text = "<"; break;
   case BOP_LTE: text = "<="; break;
   case BOP_GT: text = ">"; break;
   case BOP_GTE: text = ">="; break;
   case BOP_SHIFT_L: text = "<<"; break;
   case BOP_SHIFT_R: text = ">>"; break;
   case BOP_ADD: text = "+"; break;
   case BOP_SUB: text = "-"; break;
   case BOP_MUL: text = "*"; break;
   case BOP_DIV: text = "/"; break;
   case BOP_MOD: text = "%"; break;
   default: break;
   }
   write( codegen, " %s ", text );
   emit_operand( codegen, binary->rside );
}

static void emit_assign( struct codegen* codegen, struct assign* assign ) {
   emit_operand( codegen, assign->lside );
   //STATIC_ASSERT( BOP_TOTAL == 19 );
   const char* text = "";
   switch ( assign->op ) {
   case AOP_SIMPLE: text = "="; break;
   case AOP_ADD: text = "+="; break;
   case AOP_SUB: text = "-="; break;
   case AOP_MUL: text = "*="; break;
   case AOP_DIV: text = "/="; break;
   case AOP_MOD: text = "%="; break;
   case AOP_SHIFT_L: text = "<<="; break;
   case AOP_SHIFT_R: text = ">>="; break;
   case AOP_BIT_AND: text = "&="; break;
   case AOP_BIT_XOR: text = "^="; break;
   case AOP_BIT_OR: text = "|="; break;
   default: break;
   }
   write( codegen, " %s ", text );
   emit_operand( codegen, assign->rside );
}

static void visit_prefix( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY:
      emit_unary( codegen,
         ( struct unary* ) node );
      break;
   case NODE_INC:
      visit_inc( codegen,
         ( struct inc* ) node );
      break;
   default:
      visit_suffix( codegen, node );
   }
}

static void emit_unary( struct codegen* codegen, struct unary* unary ) {
   switch ( unary->op ) {
   case UOP_MINUS:
      write( codegen, "-" );
      break;
   case UOP_LOGICALNOT:
      write( codegen, "!" );
      if ( ! ( unary->operand->type == NODE_UNARY &&
         ( ( struct unary* ) unary->operand )->op == UOP_LOGICALNOT ) ) {
         write( codegen, " " );
      }
      break;
   case UOP_BITWISENOT:
      write( codegen, "~" );
      break;
   default:
      break;
   }
   emit_operand( codegen, unary->operand );
}

static void visit_inc( struct codegen* codegen, struct inc* inc ) {
   write( codegen, inc->decrement ? "--" : "++" );
   emit_operand( codegen, inc->operand );
}

static void visit_suffix( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_INCPOST:
      visit_post_inc( codegen,
         ( struct inc* ) node );
      break;
   case NODE_SUBSCRIPT:
      visit_subscript( codegen,
         ( struct subscript* ) node );
      break;
   case NODE_CALL:
      emit_call( codegen,
         ( struct call* ) node );
      break;
   default:
      visit_primary( codegen,
         node );
   }
}

static void visit_post_inc( struct codegen* codegen, struct inc* inc ) {
   emit_operand( codegen, inc->operand );
   write( codegen, inc->decrement ? "--" : "++" );
}

static void visit_subscript( struct codegen* codegen,
   struct subscript* subscript ) {
   emit_operand( codegen, subscript->lside );
   write( codegen, "[ " );
   emit_expr( codegen, subscript->index );
   write( codegen, " ]" );
}

static void emit_call( struct codegen* codegen, struct call* call ) {
   emit_operand( codegen, call->operand );
   write( codegen, "(" );
   if ( call->format_item ) {
      write_format_item_list( codegen, call );
   }
   else {
      if ( call->direct ) {
         write( codegen, " " );
         write( codegen, "const:" );
      }
   }
   if ( list_size( &call->args ) > 0 ) {
      write( codegen, " " );
      struct list_iter i;
      list_iterate( &call->args, &i );
      while ( ! list_end( &i ) ) {
         emit_expr( codegen, list_data( &i ) );
         if ( list_data( &i ) != list_tail( &call->args ) ) {
            write( codegen, ", " );
         }
         list_next( &i );
      }
      write( codegen, " " );
   }
   write( codegen, ")" );
}

static void write_format_item_list( struct codegen* codegen,
   struct call* call ) {
   write( codegen, " " );
   struct format_item* item = call->format_item;
   while ( item ) {
      if ( item->cast == FCAST_ARRAY ) {
         write_format_item_array( codegen, call, item );
         goto next;
      }
      const char* text = "";
      switch ( item->cast ) {
      case FCAST_DECIMAL: text = "d"; break;
      case FCAST_STRING: text = "s"; break;
      case FCAST_CHAR: text = "c"; break;
      case FCAST_FIXED: text = "f"; break;
      case FCAST_NAME: text = "n"; break;
      case FCAST_LOCAL_STRING: text = "l"; break;
      case FCAST_KEY: text = "k"; break;
      case FCAST_BINARY: text = "b"; break;
      case FCAST_HEX: text = "x"; break;
      case FCAST_ARRAY: text = "a"; break;
      default: break;
      }
      write( codegen, "%s: ", text );
      switch ( item->cast ) {
      case FCAST_STRING:
      case FCAST_LOCAL_STRING:
      case FCAST_KEY:
         if ( item->value->root->type == NODE_LITERAL ) {
            struct literal* literal = ( struct literal* ) item->value->root;
            const char* string = t_lookup_string( codegen->task,
               ( u32 ) literal->value );
            if ( string ) {
               write( codegen, "\"%s\"", string );
               break;
            }
         }
         emit_expr( codegen, item->value );
         break;
      case FCAST_CHAR:
         if ( item->value->root->type == NODE_LITERAL ) {
            struct literal* literal = ( struct literal* ) item->value->root;
            if ( isprint( literal->value ) ) {
               write( codegen, "'%c'", literal->value );
               break;
            }
         }
         emit_expr( codegen, item->value );
         break;
      default:
         emit_expr( codegen, item->value );
      }
      next:
      item = item->next;
      if ( item ) {
         write( codegen, ", " );
      }
   }
   if ( list_size( &call->args ) > 0 ) {
      write( codegen, ";" );
   }
   else {
      write( codegen, " " );
   }
}

static void write_format_item_array( struct codegen* codegen,
   struct call* call, struct format_item* item ) {
   write( codegen, "a: " );
   if ( item->extra ) {
      struct format_item_array* extra = item->extra;
      write( codegen, "( " );
      emit_expr( codegen, item->value );
      write( codegen, ", " );
      emit_expr( codegen, extra->offset );
      write( codegen, ", " );
      emit_expr( codegen, extra->length );
      write( codegen, " )" );
   }
   else {
      emit_expr( codegen, item->value );
   }
}

static void visit_primary( struct codegen* codegen, struct node* node ) {
   switch ( node->type ) {
   case NODE_LITERAL:
      emit_literal( codegen,
         ( struct literal* ) node );
      break;
   case NODE_NAMEUSAGE:
      visit_name_usage( codegen,
         ( struct name_usage* ) node );
      break;
   case NODE_ASPEC:
      emit_aspec( codegen,
         ( struct aspec* ) node );
      break;
   case NODE_FUNC:
      emit_func( codegen,
         ( struct func* ) node );
      break;
   case NODE_VAR:
      emit_var( codegen,
         ( struct var* ) node );
      break;
   case NODE_STRCPYCALL:
      visit_strcpy_call( codegen,
         ( struct strcpy_call* ) node );
      break;
   case NODE_PAREN:
      visit_paren( codegen,
         ( struct paren* ) node );
      break;
   case NODE_UNKNOWN:
      visit_unknown( codegen,
         ( struct unknown* ) node );
      break;
   case NODE_PALTRANS:
      visit_paltrans( codegen,
         ( struct paltrans* ) node );
      break;
   default:
      UNREACHABLE();
      t_bail( codegen->task );
   }
}

static void emit_literal( struct codegen* codegen, struct literal* literal ) {
   write( codegen, "%d", literal->value );
}

static void visit_name_usage( struct codegen* codegen,
   struct name_usage* usage ) {
   write( codegen, "%s", usage->name );
}

static void emit_aspec( struct codegen* codegen, struct aspec* aspec ) {
   write( codegen, aspec->name );
}

static void emit_func( struct codegen* codegen, struct func* func ) {
   // Name.
   if ( func->name.length > 0 ) {
      write( codegen, func->name.value );
   }
   else {
      switch ( func->type ) {
      case FUNC_USER:
         write( codegen, "Func%d", func->more.user->index );
         break;
      default:
         UNREACHABLE();
      }
   }
}

static void emit_var( struct codegen* codegen, struct var* var ) {
   write_var_name( codegen, var );
}

static void write_var_name( struct codegen* codegen, struct var* var ) {
   if ( var->name.length > 0 ) {
      write( codegen, "%s", var->name.value );
   }
   else {
      const char* storage = "";
      switch ( var->storage ) {
      case STORAGE_MAP: storage = "map"; break;
      case STORAGE_WORLD: storage = "world"; break;
      case STORAGE_GLOBAL: storage = "global"; break;
      default: break;
      }
      const char* layout = "var";
      if ( var->array ) {
         layout = "array";
      }
      write( codegen, "%s%s%d", storage, layout, var->index );
   }
}

static void visit_strcpy_call( struct codegen* codegen,
   struct strcpy_call* call ) {
   write( codegen, "StrCpy( a: ( " );
   emit_expr( codegen, call->array );
   write( codegen, ", " );
   emit_expr( codegen, call->array_offset );
   write( codegen, ", " );
   emit_expr( codegen, call->array_length );
   write( codegen, " ), " );
   emit_expr( codegen, call->string );
   write( codegen, ", " );
   emit_expr( codegen, call->offset );
   write( codegen, " )" );
}

static void visit_paren( struct codegen* codegen, struct paren* paren ) {
   write( codegen, "( " );
   emit_operand( codegen, paren->contents );
   write( codegen, " )" );
}

static void visit_unknown( struct codegen* codegen, struct unknown* unknown ) {
   switch ( unknown->type ) {
   case UNKNOWN_ASPEC:
      write( codegen, "// ActionSpecial_%d", unknown->more.aspec.id );
      break;
   case UNKNOWN_EXT:
      write( codegen, "// ExtFunc_%d", unknown->more.ext.id );
      break;
   }
}

static void visit_paltrans( struct codegen* codegen, struct paltrans* trans ) {
   write( codegen, "CreateTranslation( " );
   emit_expr( codegen, trans->number );
   if ( list_size( &trans->ranges ) > 0 ) {
      write( codegen, ", " );
   }
   struct list_iter i;
   list_iterate( &trans->ranges, &i );
   while ( ! list_end( &i ) ) {
      struct palrange* range = list_data( &i );
      emit_expr( codegen, range->begin );
      write( codegen, ":" );
      emit_expr( codegen, range->end );
      write( codegen, "=" );
      switch ( range->type ) {
      case PALRANGE_COLON:
         emit_expr( codegen, range->value.colon.begin );
         write( codegen, ":" );
         emit_expr( codegen, range->value.colon.end );
         break;
      case PALRANGE_RGB:
      case PALRANGE_SATURATED:
         if ( range->type == PALRANGE_SATURATED ) {
            write( codegen, "%%" );
         }
         write( codegen, "[ " );
         emit_expr( codegen, range->value.rgb.red1 );
         write( codegen, ", " );
         emit_expr( codegen, range->value.rgb.green1 );
         write( codegen, ", " );
         emit_expr( codegen, range->value.rgb.blue1 );
         write( codegen, " ]:[ " );
         emit_expr( codegen, range->value.rgb.red2 );
         write( codegen, ", " );
         emit_expr( codegen, range->value.rgb.green2 );
         write( codegen, ", " );
         emit_expr( codegen, range->value.rgb.blue2 );
         write( codegen, " ]" );
         break;
      case PALRANGE_COLORISATION:
         write( codegen, "#[ " );
         emit_expr( codegen, range->value.colorisation.red );
         write( codegen, ", " );
         emit_expr( codegen, range->value.colorisation.green );
         write( codegen, ", " );
         emit_expr( codegen, range->value.colorisation.blue );
         write( codegen, " ]" );
         break;
      case PALRANGE_TINT:
         write( codegen, "@" );
         emit_expr( codegen, range->value.tint.amount );
         write( codegen, "[ " );
         emit_expr( codegen, range->value.tint.red );
         write( codegen, ", " );
         emit_expr( codegen, range->value.tint.green );
         write( codegen, ", " );
         emit_expr( codegen, range->value.tint.blue );
         write( codegen, " ]" );
         break;
      default:
         UNREACHABLE();
         t_bail( codegen->task );
      }
      list_next( &i );
      if ( ! list_end( &i ) ) {
         write( codegen, ", " );
      }
   }
   write( codegen, " )" );
}
