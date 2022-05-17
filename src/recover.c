/*

   Stage 3 -- Recovery

   In the "Recovery" stage, we construct an AST (abstract syntax tree) from
   the annotated p-code.

*/

#include <stdio.h>

#include "task.h"
#include "pcode.h"

struct recovery {
   struct task* task;
   struct script* script;
   struct func* func;
};

struct stmt_recovery {
   struct pcode_range range;
   struct node* output_node;
   struct block* block;
};

struct expr_recovery {
   struct pcode_range range;
   struct format_call_recovery* format_call;
   struct paltrans* trans;
   struct expr* output_node;
   struct list stack;
   bool done;
};

struct format_call_recovery {
   struct func* func;
   struct format_item* format_item;
   struct format_item* format_item_tail;
   u32 args_start;
};

struct operand {
   struct node* node;
   enum precedence {
      PRECEDENCE_BOTTOM,
      PRECEDENCE_ASSIGN,
      PRECEDENCE_CONDITIONAL,
      PRECEDENCE_LOGOR,
      PRECEDENCE_LOGAND,
      PRECEDENCE_BITOR,
      PRECEDENCE_BITXOR,
      PRECEDENCE_BITAND,
      PRECEDENCE_EQ,
      PRECEDENCE_LT,
      PRECEDENCE_SHIFT,
      PRECEDENCE_ADD,
      PRECEDENCE_MUL,
      PRECEDENCE_TOP,
   } precedence;
};

static void init_recovery( struct recovery* recovery, struct task* task );
static void recover_script_list( struct recovery* recovery );
static void recover_script( struct recovery* recovery,
   struct script* script );
static void recover_script_param_list( struct recovery* recovery );
static void recover_script_param( struct recovery* recovery,
   i32 param_number );
static void recover_func_list( struct recovery* recovery );
static void recover_func( struct recovery* recovery, struct func* func );
static void init_stmt_recovery( struct stmt_recovery* recovery,
   struct pcode* start, struct pcode* end );
static void recover_block( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct block* alloc_block( void );
static void recover_stmt( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static void examine_note( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static void recover_if( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static void recover_switch( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct switch_stmt* alloc_switch_stmt( void );
static void recover_case( struct stmt_recovery* recovery );
static struct if_stmt* alloc_if_stmt( void );
static void recover_loop( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct while_stmt* alloc_while_stmt( void );
static void recover_do( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct do_stmt* alloc_do_stmt( void );
static void recover_for( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct for_stmt* alloc_for( void );
static void recover_jump( struct stmt_recovery* recovery );
static void recover_script_jump( struct stmt_recovery* recovery );
static struct script_jump* alloc_script_jump( void );
static void recover_return( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static struct return_stmt* alloc_return( void );
static void examine_returnvoid( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );
static void recover_inline_asm( struct stmt_recovery* recovery );
static void init_expr_recovery( struct expr_recovery* recovery,
   struct pcode* start, struct pcode* end );
static void recover_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_operand( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct expr* alloc_expr( void );
static void push( struct expr_recovery* recovery, struct node* node,
   enum precedence precedence );
static struct node* pop( struct recovery* recovery,
   struct expr_recovery* expr_recovery, enum precedence parent_precedence );
static void recover_unary( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_minus( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_inc( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_array_inc( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_pushvar( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_pusharray( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_post_inc( struct expr_recovery* expr_recovery,
   struct node* operand );
static void recover_assign( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_assign_array( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct node* recover_var( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct var* alloc_var( void );
static void recover_binary( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct paren* alloc_paren( void );
static i32 get_binary_precedence( struct binary* binary );
static struct node* parenthesize_binary_operand( struct node* operand,
   i32 parent_precedence );
static void recover_call_aspec( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct unknown* alloc_unknown( void );
static void recover_call_ext( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_call_intern( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_call_ded( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_call_ded_direct( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct call* alloc_call( void );
static void examine_beginprint( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_printvalue( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct format_item* alloc_format_item( void );
static void append_format_item( struct format_call_recovery* recovery,
   struct format_item* item );
static void examine_printarray( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_endprint( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_strcpy( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_starttranslation( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static struct expr* pop_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_translationrange( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void examine_endtranslation( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_call_user( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_literal( struct expr_recovery* recovery );
static struct literal* alloc_literal( void );
static void examine_dup( struct recovery* recovery,
   struct expr_recovery* expr_recovery );
static void recover_expr_stmt( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery );

static void init_range( struct pcode_range* range, struct pcode* start,
   struct pcode* end );
static bool have_pcode( struct pcode_range* range );
static void seek_pcode( struct pcode_range* range, struct pcode* pcode );
static void next_pcode( struct pcode_range* range );

void t_recover( struct task* task ) {
   struct recovery recovery;
   init_recovery( &recovery, task );
   recover_script_list( &recovery );
   recover_func_list( &recovery );
}

static void init_recovery( struct recovery* recovery, struct task* task ) {
   recovery->task = task;
   recovery->script = NULL;
   recovery->func = NULL;
}

static void recover_script_list( struct recovery* recovery ) {
   struct list_iter i;
   list_iterate( &recovery->task->scripts, &i );
   while ( ! list_end( &i ) ) {
      recover_script( recovery, list_data( &i ) );
      list_next( &i );
   }
}

static void recover_script( struct recovery* recovery,
   struct script* script ) {
   recovery->script = script;
   recover_script_param_list( recovery );
   struct stmt_recovery body;
   init_stmt_recovery( &body,
      script->body_start,
      script->body_end );
   recover_block( recovery, &body );
   script->body = body.block;
   recovery->script = NULL;
}

static void recover_script_param_list( struct recovery* recovery ) {
   u32 param_number = 0;
   while ( param_number < recovery->script->num_param ) {
      recover_script_param( recovery, ( i32 ) param_number );
      ++param_number;
   }
}

static void recover_script_param( struct recovery* recovery,
   i32 param_number ) {
   struct var* var = alloc_var();
   var->index = ( u32 ) param_number;
   recovery->script->vars[ param_number ] = var;
   const char* name = NULL;
   switch ( recovery->script->type ) {
   case SCRIPT_TYPE_DISCONNECT:
      switch ( param_number ) {
      case 0: name = "player"; break;
      default: break;
      }
      break;
   case SCRIPT_TYPE_EVENT:
      switch ( param_number ) {
      case 0: name = "type"; break;
      case 1: name = "arg1"; break;
      case 2: name = "arg2"; break;
      default: break;
      }
      break;
   default:
      break;
   }
   if ( name ) {
      str_append( &var->name, name );
   }
}

static void recover_func_list( struct recovery* recovery ) {
   struct list_iter i;
   list_iterate( &recovery->task->funcs, &i );
   while ( ! list_end( &i ) ) {
      recover_func( recovery, list_data( &i ) );
      list_next( &i );
   }
}

static void recover_func( struct recovery* recovery, struct func* func ) {
   recovery->func = func;
   struct stmt_recovery body;
   init_stmt_recovery( &body,
      func->more.user->start,
      func->more.user->end );
   recover_block( recovery, &body );
   func->more.user->body = body.block;
   recovery->func = NULL;
}

static void init_stmt_recovery( struct stmt_recovery* recovery,
   struct pcode* start, struct pcode* end ) {
   init_range( &recovery->range, start, end );
   recovery->output_node = NULL;
}

static void recover_block( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct block* block = alloc_block(); 
   while ( have_pcode( &stmt_recovery->range ) ) {
      recover_stmt( recovery, stmt_recovery );
      if ( ! stmt_recovery->output_node ) {
         break;
      }
      list_append( &block->stmts, stmt_recovery->output_node );
   }
   stmt_recovery->output_node = &block->node;
   stmt_recovery->block = block;
}

static struct block* alloc_block( void ) {
   struct block* block = mem_alloc( sizeof( *block ) );
   block->node.type = NODE_BLOCK;
   list_init( &block->stmts );
   return block;
}

static void recover_stmt( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   stmt_recovery->output_node = NULL;
   if ( stmt_recovery->range.pcode->note ) {
      examine_note( recovery, stmt_recovery );
   }
   else {
      switch ( stmt_recovery->range.pcode->opcode ) {
      case PCD_TERMINATE:
      case PCD_RESTART:
      case PCD_SUSPEND:
         recover_script_jump( stmt_recovery );
         break;
      case PCD_RETURNVOID:
         examine_returnvoid( recovery, stmt_recovery );
         break;
      case PCD_NOP:
         next_pcode( &stmt_recovery->range );
         break;
      default:
         recover_inline_asm( stmt_recovery );
      }
   }
}

static void examine_note( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct pcode* pcode = stmt_recovery->range.pcode;
   switch ( stmt_recovery->range.pcode->note->type ) {
   case NOTE_IF:
      recover_if( recovery, stmt_recovery );
      break;
   case NOTE_SWITCH:
      recover_switch( recovery, stmt_recovery );
      break;
   case NOTE_CASE:
      recover_case( stmt_recovery );
      break;
   case NOTE_LOOP:
      recover_loop( recovery, stmt_recovery );
      break;
   case NOTE_DO:
      recover_do( recovery, stmt_recovery );
      break;
   case NOTE_FOR:
      recover_for( recovery, stmt_recovery );
      break;
   case NOTE_JUMP:
      recover_jump( stmt_recovery );
      break;
   case NOTE_RETURN:
      recover_return( recovery, stmt_recovery );
      break;
   case NOTE_EXPRSTMT:
      recover_expr_stmt( recovery, stmt_recovery );
      break;
   default:
      UNREACHABLE();
      t_bail( recovery->task );
   }
   pcode->note = pcode->note->next;
}

static void recover_if( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct if_note* note = ( struct if_note* ) stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->cond_start, note->cond_end );
   recover_expr( recovery, &expr );
   struct stmt_recovery body;
   init_stmt_recovery( &body, note->body_start, note->body_end );
   recover_block( recovery, &body );
   struct if_stmt* stmt = alloc_if_stmt();
   stmt->cond = expr.output_node;
   stmt->body = body.block;
   stmt->else_body = NULL;
   if ( note->else_body_start ) {
      init_stmt_recovery( &body,
         note->else_body_start,
         note->else_body_end );
      recover_block( recovery, &body );
      stmt->else_body = body.block;
   }
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}

static struct if_stmt* alloc_if_stmt( void ) {
   struct if_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_IF;
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->else_body = NULL;
   return stmt;
}

static void recover_switch( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct switch_note* note = ( struct switch_note* )
      stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr,
      note->cond_start,
      note->cond_end );
   recover_expr( recovery, &expr );
   struct stmt_recovery body;
   init_stmt_recovery( &body,
      note->body_start,
      note->body_end );
   recover_block( recovery, &body );
   struct switch_stmt* stmt = alloc_switch_stmt();
   stmt->cond = expr.output_node;
   stmt->body = body.block;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}

static struct switch_stmt* alloc_switch_stmt( void ) {
   struct switch_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_SWITCH;
   stmt->cond = NULL;
   stmt->case_head = NULL;
   stmt->body = NULL;
   return stmt;
}

static void recover_case( struct stmt_recovery* recovery ) {
   struct case_note* note = ( struct case_note* ) recovery->range.pcode->note;
   struct case_label* label = mem_alloc( sizeof( *label ) );
   label->node.type = ( note->default_case ) ? NODE_CASEDEFAULT : NODE_CASE;
   label->value = note->value;
   recovery->output_node = &label->node;
}

static void recover_loop( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct loop_note* note = ( struct loop_note* )
      stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->cond_start, note->cond_end );
   recover_expr( recovery, &expr );
   struct stmt_recovery body;
   init_stmt_recovery( &body, note->body_start, note->body_end );
   recover_block( recovery, &body );
   struct while_stmt* stmt = alloc_while_stmt();
   stmt->cond = expr.output_node;
   stmt->body = body.block;
   stmt->until = note->until;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}

static struct while_stmt* alloc_while_stmt( void ) {
   struct while_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_WHILE;
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->until = false;
   return stmt;
}

static void recover_do( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct do_note* note = ( struct do_note* ) stmt_recovery->range.pcode->note;
   stmt_recovery->range.pcode->note = NULL;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->cond_start, note->cond_end );
   recover_expr( recovery, &expr );
   struct stmt_recovery body;
   init_stmt_recovery( &body, note->body_start, note->body_end );
   recover_block( recovery, &body );
   struct do_stmt* stmt = alloc_do_stmt();
   stmt->cond = expr.output_node;
   stmt->body = body.block;
   stmt->until = note->until;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
   note->body_start->note = &note->note;
}

static struct do_stmt* alloc_do_stmt( void ) {
   struct do_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_DO;
   stmt->cond = NULL;
   stmt->body = NULL;
   stmt->until = false;
   return stmt;
}

static void recover_for( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct for_note* note = ( struct for_note* )
      stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->cond_start, note->cond_end );
   recover_expr( recovery, &expr );
   struct list post;
   list_init( &post );
   struct list_iter i;
   list_iterate( &note->post, &i );
   while ( ! list_end( &i ) ) {
      struct for_note_post* post_expr = list_data( &i );
      struct expr_recovery expr;
      init_expr_recovery( &expr, post_expr->start, post_expr->end );
      recover_expr( recovery, &expr );
      list_append( &post, expr.output_node );
      list_next( &i );
   }
   struct stmt_recovery body;
   init_stmt_recovery( &body, note->body_start, note->body_end );
   recover_block( recovery, &body );
   struct for_stmt* stmt = alloc_for();
   stmt->cond = expr.output_node;
   stmt->post = post;
   stmt->body = body.block;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}

static struct for_stmt* alloc_for( void ) {
   struct for_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_FOR;
   stmt->cond = NULL;
   list_init( &stmt->post );
   stmt->body = NULL;
   return stmt;
}

static void recover_jump( struct stmt_recovery* recovery ) {
   struct jump_note* note = ( struct jump_note* ) recovery->range.pcode->note;
   struct jump* jump = mem_alloc( sizeof( *jump ) );
   jump->node.type = NODE_JUMP;
   jump->type = JUMP_BREAK;
   if ( note->stmt == JUMPNOTE_CONTINUE ) {
      jump->type = JUMP_CONTINUE;
   }
   recovery->output_node = &jump->node;
   next_pcode( &recovery->range );
}

static void recover_script_jump( struct stmt_recovery* recovery ) {
   struct script_jump* jump = alloc_script_jump();
   switch ( recovery->range.pcode->opcode ) {
   case PCD_RESTART:
      jump->type = SCRIPTJUMP_RESTART;
      break;
   case PCD_SUSPEND:
      jump->type = SCRIPTJUMP_SUSPEND;
      break;
   default:
      break;
   }
   recovery->output_node = &jump->node;
   next_pcode( &recovery->range );
}

static struct script_jump* alloc_script_jump( void ) {
   struct script_jump* jump = mem_alloc( sizeof( *jump ) );
   jump->node.type = NODE_SCRIPTJUMP;
   jump->type = SCRIPTJUMP_TERMINATE;
   return jump;
}

static void recover_return( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct return_note* note = ( struct return_note* )
      stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->expr_start, note->expr_end );
   recover_expr( recovery, &expr );
   struct return_stmt* stmt = alloc_return();
   stmt->return_value = expr.output_node;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}

static struct return_stmt* alloc_return( void ) {
   struct return_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_RETURN;
   stmt->return_value = NULL;
   return stmt;
}

static void examine_returnvoid( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct return_stmt* stmt = alloc_return();
   stmt_recovery->output_node = &stmt->node;
   next_pcode( &stmt_recovery->range );
}

static void recover_inline_asm( struct stmt_recovery* recovery ) {
   struct inline_asm* inline_asm = mem_alloc( sizeof( *inline_asm ) );
   inline_asm->node.type = NODE_INLINEASM;
   inline_asm->pcode = recovery->range.pcode;
   recovery->output_node = &inline_asm->node;
   next_pcode( &recovery->range );
}

static void init_expr_recovery( struct expr_recovery* recovery,
   struct pcode* start, struct pcode* end ) {
   init_range( &recovery->range, start, end );
   recovery->format_call = NULL;
   recovery->trans = NULL;
   recovery->output_node = NULL;
   list_init( &recovery->stack );
   recovery->done = false;
}

static struct expr* alloc_expr( void ) {
   struct expr* expr = mem_alloc( sizeof( *expr ) );
   expr->node.type = NODE_EXPR;
   expr->root = NULL;
   return expr;
}

static void push( struct expr_recovery* recovery, struct node* node,
   enum precedence precedence ) {
   struct operand* operand = mem_alloc( sizeof( *operand ) );
   operand->node = node;
   operand->precedence = precedence;
   list_prepend( &recovery->stack, operand );
}

static struct node* pop( struct recovery* recovery,
   struct expr_recovery* expr_recovery, enum precedence parent_precedence ) {
   if ( list_size( &expr_recovery->stack ) == 0 ) {
      t_diag( recovery->task, DIAG_INTERNAL | DIAG_ERR,
         "attempting to pop operand, but stack size is 0" );
      t_bail( recovery->task );
   }
   struct operand* operand = list_shift( &expr_recovery->stack );
   if ( operand->precedence < parent_precedence ) {
      struct paren* paren = alloc_paren();
      paren->contents = operand->node;
      operand->node = &paren->node;
   }
   struct node* node = operand->node;
   mem_free( operand );
   return node;
}

static void recover_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   examine_expr( recovery, expr_recovery );
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   expr_recovery->output_node = expr;
   if ( list_size( &expr_recovery->stack ) != 0 ) {
      t_diag( recovery->task, DIAG_INTERNAL | DIAG_ERR,
         "stack size not 0" );
      t_bail( recovery->task );
   }
}

static void examine_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   while ( have_pcode( &expr_recovery->range ) && ! expr_recovery->done ) {
      recover_operand( recovery, expr_recovery );
   }
   expr_recovery->done = false;
}

static void recover_operand( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_ORLOGICAL:
   case PCD_ANDLOGICAL:
   case PCD_ORBITWISE:
   case PCD_EORBITWISE:
   case PCD_ANDBITWISE:
   case PCD_EQ:
   case PCD_NE:
   case PCD_LT:
   case PCD_LE:
   case PCD_GT:
   case PCD_GE:
   case PCD_LSHIFT:
   case PCD_RSHIFT:
   case PCD_ADD:
   case PCD_SUBTRACT:
   case PCD_MULTIPLY:
   case PCD_DIVIDE:
   case PCD_MODULUS:
      recover_binary( recovery, expr_recovery );
      break;
   case PCD_NEGATELOGICAL:
   case PCD_NEGATEBINARY:
      recover_unary( recovery, expr_recovery );
      break;
   case PCD_UNARYMINUS:
      recover_minus( recovery, expr_recovery );
      break;
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_INCGLOBALVAR:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
   case PCD_DECGLOBALVAR:
      recover_inc( recovery, expr_recovery );
      break;
   case PCD_INCSCRIPTARRAY:
   case PCD_INCMAPARRAY:
   case PCD_INCWORLDARRAY:
   case PCD_INCGLOBALARRAY:
   case PCD_DECSCRIPTARRAY:
   case PCD_DECMAPARRAY:
   case PCD_DECWORLDARRAY:
   case PCD_DECGLOBALARRAY:
      recover_array_inc( recovery, expr_recovery );
      break;
   case PCD_ASSIGNSCRIPTVAR:
   case PCD_ADDSCRIPTVAR:
   case PCD_SUBSCRIPTVAR:
   case PCD_MULSCRIPTVAR:
   case PCD_DIVSCRIPTVAR:
   case PCD_MODSCRIPTVAR:
   case PCD_ANDSCRIPTVAR:
   case PCD_EORSCRIPTVAR:
   case PCD_ORSCRIPTVAR:
   case PCD_LSSCRIPTVAR:
   case PCD_RSSCRIPTVAR:
   case PCD_ASSIGNMAPVAR:
   case PCD_ADDMAPVAR:
   case PCD_SUBMAPVAR:
   case PCD_MULMAPVAR:
   case PCD_DIVMAPVAR:
   case PCD_MODMAPVAR:
   case PCD_ANDMAPVAR:
   case PCD_EORMAPVAR:
   case PCD_ORMAPVAR:
   case PCD_LSMAPVAR:
   case PCD_RSMAPVAR:
   case PCD_ASSIGNWORLDVAR:
   case PCD_ADDWORLDVAR:
   case PCD_SUBWORLDVAR:
   case PCD_MULWORLDVAR:
   case PCD_DIVWORLDVAR:
   case PCD_MODWORLDVAR:
   case PCD_ANDWORLDVAR:
   case PCD_EORWORLDVAR:
   case PCD_ORWORLDVAR:
   case PCD_LSWORLDVAR:
   case PCD_RSWORLDVAR:
   case PCD_ASSIGNGLOBALVAR:
   case PCD_ADDGLOBALVAR:
   case PCD_SUBGLOBALVAR:
   case PCD_MULGLOBALVAR:
   case PCD_DIVGLOBALVAR:
   case PCD_MODGLOBALVAR:
   case PCD_ANDGLOBALVAR:
   case PCD_EORGLOBALVAR:
   case PCD_ORGLOBALVAR:
   case PCD_LSGLOBALVAR:
   case PCD_RSGLOBALVAR:
      recover_assign( recovery, expr_recovery );
      break;
   case PCD_ASSIGNSCRIPTARRAY:
   case PCD_ADDSCRIPTARRAY:
   case PCD_SUBSCRIPTARRAY:
   case PCD_MULSCRIPTARRAY:
   case PCD_DIVSCRIPTARRAY:
   case PCD_MODSCRIPTARRAY:
   case PCD_ANDSCRIPTARRAY:
   case PCD_EORSCRIPTARRAY:
   case PCD_ORSCRIPTARRAY:
   case PCD_LSSCRIPTARRAY:
   case PCD_RSSCRIPTARRAY:
   case PCD_ASSIGNMAPARRAY:
   case PCD_ADDMAPARRAY:
   case PCD_SUBMAPARRAY:
   case PCD_MULMAPARRAY:
   case PCD_DIVMAPARRAY:
   case PCD_MODMAPARRAY:
   case PCD_ANDMAPARRAY:
   case PCD_EORMAPARRAY:
   case PCD_ORMAPARRAY:
   case PCD_LSMAPARRAY:
   case PCD_RSMAPARRAY:
   case PCD_ASSIGNWORLDARRAY:
   case PCD_ADDWORLDARRAY:
   case PCD_SUBWORLDARRAY:
   case PCD_MULWORLDARRAY:
   case PCD_DIVWORLDARRAY:
   case PCD_MODWORLDARRAY:
   case PCD_ANDWORLDARRAY:
   case PCD_EORWORLDARRAY:
   case PCD_ORWORLDARRAY:
   case PCD_LSWORLDARRAY:
   case PCD_RSWORLDARRAY:
   case PCD_ASSIGNGLOBALARRAY:
   case PCD_ADDGLOBALARRAY:
   case PCD_SUBGLOBALARRAY:
   case PCD_MULGLOBALARRAY:
   case PCD_DIVGLOBALARRAY:
   case PCD_MODGLOBALARRAY:
   case PCD_ANDGLOBALARRAY:
   case PCD_EORGLOBALARRAY:
   case PCD_ORGLOBALARRAY:
   case PCD_LSGLOBALARRAY:
   case PCD_RSGLOBALARRAY:
      recover_assign_array( recovery, expr_recovery );
      break;
   case PCD_LSPEC1:
   case PCD_LSPEC2:
   case PCD_LSPEC3:
   case PCD_LSPEC4:
   case PCD_LSPEC5:
   case PCD_LSPEC1DIRECT:
   case PCD_LSPEC2DIRECT:
   case PCD_LSPEC3DIRECT:
   case PCD_LSPEC4DIRECT:
   case PCD_LSPEC5DIRECT:
   case PCD_LSPEC1DIRECTB:
   case PCD_LSPEC2DIRECTB:
   case PCD_LSPEC3DIRECTB:
   case PCD_LSPEC4DIRECTB:
   case PCD_LSPEC5DIRECTB:
   case PCD_LSPEC5RESULT:
   case PCD_LSPEC5EX:
   case PCD_LSPEC5EXRESULT:
      recover_call_aspec( recovery, expr_recovery );
      break;
   case PCD_CALLFUNC:
      recover_call_ext( recovery, expr_recovery );
      break;
   case PCD_DELAY:
   case PCD_RANDOM:
   case PCD_THINGCOUNT:
   case PCD_TAGWAIT:
   case PCD_POLYWAIT:
   case PCD_CHANGEFLOOR:
   case PCD_CHANGECEILING:
   case PCD_LINESIDE:
   case PCD_SCRIPTWAIT:
   case PCD_CLEARLINESPECIAL:
   case PCD_PLAYERCOUNT:
   case PCD_GAMETYPE:
   case PCD_GAMESKILL:
   case PCD_TIMER:
   case PCD_SECTORSOUND:
   case PCD_AMBIENTSOUND:
   case PCD_SOUNDSEQUENCE:
   case PCD_SETLINETEXTURE:
   case PCD_SETLINEBLOCKING:
   case PCD_SETLINESPECIAL:
   case PCD_THINGSOUND:
   case PCD_ACTIVATORSOUND:
   case PCD_LOCALAMBIENTSOUND:
   case PCD_SETLINEMONSTERBLOCKING:
   case PCD_ISNETWORKGAME:
   case PCD_PLAYERTEAM:
   case PCD_PLAYERHEALTH:
   case PCD_PLAYERARMORPOINTS:
   case PCD_PLAYERFRAGS:
   case PCD_BLUETEAMCOUNT:
   case PCD_REDTEAMCOUNT:
   case PCD_BLUETEAMSCORE:
   case PCD_REDTEAMSCORE:
   case PCD_ISONEFLAGCTF:
   case PCD_GETINVASIONWAVE:
   case PCD_GETINVASIONSTATE:
   case PCD_MUSICCHANGE:
   case PCD_CONSOLECOMMAND:
   case PCD_SINGLEPLAYER:
   case PCD_FIXEDMUL:
   case PCD_FIXEDDIV:
   case PCD_SETGRAVITY:
   case PCD_SETAIRCONTROL:
   case PCD_CLEARINVENTORY:
   case PCD_GIVEINVENTORY:
   case PCD_TAKEINVENTORY:
   case PCD_CHECKINVENTORY:
   case PCD_SPAWN:
   case PCD_SPAWNSPOT:
   case PCD_SETMUSIC:
   case PCD_LOCALSETMUSIC:
   case PCD_SETFONT:
   case PCD_SETTHINGSPECIAL:
   case PCD_FADETO:
   case PCD_FADERANGE:
   case PCD_CANCELFADE:
   case PCD_PLAYMOVIE:
   case PCD_SETFLOORTRIGGER:
   case PCD_SETCEILINGTRIGGER:
   case PCD_GETACTORX:
   case PCD_GETACTORY:
   case PCD_GETACTORZ:
   case PCD_SIN:
   case PCD_COS:
   case PCD_VECTORANGLE:
   case PCD_CHECKWEAPON:
   case PCD_SETWEAPON:
   case PCD_SETMARINEWEAPON:
   case PCD_SETACTORPROPERTY:
   case PCD_GETACTORPROPERTY:
   case PCD_PLAYERNUMBER:
   case PCD_ACTIVATORTID:
   case PCD_SETMARINESPRITE:
   case PCD_GETSCREENWIDTH:
   case PCD_GETSCREENHEIGHT:
   case PCD_THINGPROJECTILE2:
   case PCD_STRLEN:
   case PCD_SETHUDSIZE:
   case PCD_GETCVAR:
   case PCD_SETRESULTVALUE:
   case PCD_GETLINEROWOFFSET:
   case PCD_GETACTORFLOORZ:
   case PCD_GETACTORANGLE:
   case PCD_GETSECTORFLOORZ:
   case PCD_GETSECTORCEILINGZ:
   case PCD_GETSIGILPIECES:
   case PCD_GETLEVELINFO:
   case PCD_CHANGESKY:
   case PCD_PLAYERINGAME:
   case PCD_PLAYERISBOT:
   case PCD_SETCAMERATOTEXTURE:
   case PCD_GETAMMOCAPACITY:
   case PCD_SETAMMOCAPACITY:
   case PCD_SETACTORANGLE:
   case PCD_SPAWNPROJECTILE:
   case PCD_GETSECTORLIGHTLEVEL:
   case PCD_GETACTORCEILINGZ:
   case PCD_SETACTORPOSITION:
   case PCD_CLEARACTORINVENTORY:
   case PCD_GIVEACTORINVENTORY:
   case PCD_TAKEACTORINVENTORY:
   case PCD_CHECKACTORINVENTORY:
   case PCD_THINGCOUNTNAME:
   case PCD_SPAWNSPOTFACING:
   case PCD_PLAYERCLASS:
   case PCD_GETPLAYERINFO:
   case PCD_CHANGELEVEL:
   case PCD_SECTORDAMAGE:
   case PCD_REPLACETEXTURES:
   case PCD_GETACTORPITCH:
   case PCD_SETACTORPITCH:
   case PCD_SETACTORSTATE:
   case PCD_THINGDAMAGE2:
   case PCD_USEINVENTORY:
   case PCD_USEACTORINVENTORY:
   case PCD_CHECKACTORCEILINGTEXTURE:
   case PCD_CHECKACTORFLOORTEXTURE:
   case PCD_GETACTORLIGHTLEVEL:
   case PCD_SETMUGSHOTSTATE:
   case PCD_THINGCOUNTSECTOR:
   case PCD_THINGCOUNTNAMESECTOR:
   case PCD_CHECKPLAYERCAMERA:
   case PCD_MORPHACTOR:
   case PCD_UNMORPHACTOR:
   case PCD_GETPLAYERINPUT:
   case PCD_CLASSIFYACTOR:
   case PCD_SCRIPTWAITNAMED:
      recover_call_ded( recovery, expr_recovery );
      break;
   case PCD_DELAYDIRECT:
   case PCD_RANDOMDIRECT:
   case PCD_THINGCOUNTDIRECT:
   case PCD_TAGWAITDIRECT:
   case PCD_POLYWAITDIRECT:
   case PCD_CHANGEFLOORDIRECT:
   case PCD_CHANGECEILINGDIRECT:
   case PCD_SCRIPTWAITDIRECT:
   case PCD_CONSOLECOMMANDDIRECT:
   case PCD_SETGRAVITYDIRECT:
   case PCD_SETAIRCONTROLDIRECT:
   case PCD_GIVEINVENTORYDIRECT:
   case PCD_TAKEINVENTORYDIRECT:
   case PCD_CHECKINVENTORYDIRECT:
   case PCD_SPAWNDIRECT:
   case PCD_SPAWNSPOTDIRECT:
   case PCD_SETMUSICDIRECT:
   case PCD_LOCALSETMUSICDIRECT:
   case PCD_SETSTYLEDIRECT:
   case PCD_SETFONTDIRECT:
   case PCD_DELAYDIRECTB:
   case PCD_RANDOMDIRECTB:
      recover_call_ded_direct( recovery, expr_recovery );
      break;
   case PCD_CALL:
   case PCD_CALLDISCARD:
      recover_call_user( recovery, expr_recovery );
      break;
   case PCD_PUSHNUMBER:
   case PCD_PUSHBYTE:
   case PCD_PUSH2BYTES:
   case PCD_PUSH3BYTES:
   case PCD_PUSH4BYTES:
   case PCD_PUSH5BYTES:
   case PCD_PUSHBYTES:
      recover_literal( expr_recovery );
      break;
   case PCD_DUP:
      examine_dup( recovery, expr_recovery );
      break;
   case PCD_PUSHSCRIPTVAR:
   case PCD_PUSHMAPVAR:
   case PCD_PUSHWORLDVAR:
      examine_pushvar( recovery, expr_recovery );
      break;
   case PCD_PUSHSCRIPTARRAY:
   case PCD_PUSHMAPARRAY:
   case PCD_PUSHWORLDARRAY:
   case PCD_PUSHGLOBALARRAY:
      examine_pusharray( recovery, expr_recovery );
      break;
   case PCD_BEGINPRINT:
      examine_beginprint( recovery, expr_recovery );
      break;
   case PCD_PRINTSTRING:
   case PCD_PRINTNUMBER:
   case PCD_PRINTCHARACTER:
   case PCD_PRINTFIXED:
   case PCD_PRINTNAME:
   case PCD_PRINTLOCALIZED:
   case PCD_PRINTBIND:
   case PCD_PRINTBINARY:
   case PCD_PRINTHEX:
      examine_printvalue( recovery, expr_recovery );
      break;
   case PCD_PRINTMAPCHARARRAY:
   case PCD_PRINTMAPCHRANGE:
   case PCD_PRINTWORLDCHARARRAY:
   case PCD_PRINTWORLDCHRANGE:
   case PCD_PRINTGLOBALCHARARRAY:
   case PCD_PRINTGLOBALCHRANGE:
      examine_printarray( recovery, expr_recovery );
      break;
   case PCD_MOREHUDMESSAGE:
      expr_recovery->format_call->args_start =
         list_size( &expr_recovery->stack );
      next_pcode( &expr_recovery->range );
      break;
   case PCD_OPTHUDMESSAGE:
      next_pcode( &expr_recovery->range );
      break;
   case PCD_ENDPRINT:
   case PCD_ENDPRINTBOLD:
   case PCD_ENDHUDMESSAGE:
   case PCD_ENDHUDMESSAGEBOLD:
   case PCD_ENDLOG:
   case PCD_SAVESTRING:
      examine_endprint( recovery, expr_recovery );
      break;
   case PCD_STRCPYTOMAPCHRANGE:
   case PCD_STRCPYTOWORLDCHRANGE:
   case PCD_STRCPYTOGLOBALCHRANGE:
      examine_strcpy( recovery, expr_recovery );
      break;
   case PCD_STARTTRANSLATION:
      examine_starttranslation( recovery, expr_recovery );
      break;
   case PCD_TRANSLATIONRANGE1:
   case PCD_TRANSLATIONRANGE2:
   case PCD_TRANSLATIONRANGE3:
   case PCD_TRANSLATIONRANGE4:
   case PCD_TRANSLATIONRANGE5:
      examine_translationrange( recovery, expr_recovery );
      break;
   case PCD_ENDTRANSLATION:
      examine_endtranslation( recovery, expr_recovery );
      break;
   case PCD_TAGSTRING:
      next_pcode( &expr_recovery->range );
      break;
   default:
      t_diag( recovery->task, DIAG_INTERNAL | DIAG_ERR,
         "unhandled pcode: %d (%s:%d)", expr_recovery->range.pcode->opcode,
         __FILE__, __LINE__ );
      t_bail( recovery->task );
   }
}

static void recover_binary( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct binary* binary = mem_alloc( sizeof( *binary ) );
   binary->node.type = NODE_BINARY;
   binary->op = BOP_NONE;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_ANDLOGICAL: binary->op = BOP_LOG_AND; break;
   case PCD_ORLOGICAL: binary->op = BOP_LOG_OR; break;
   case PCD_ORBITWISE: binary->op = BOP_BIT_OR; break;
   case PCD_EORBITWISE: binary->op = BOP_BIT_XOR; break;
   case PCD_ANDBITWISE: binary->op = BOP_BIT_AND; break;
   case PCD_EQ: binary->op = BOP_EQ; break;
   case PCD_NE: binary->op = BOP_NEQ; break;
   case PCD_LT: binary->op = BOP_LT; break;
   case PCD_LE: binary->op = BOP_LTE; break;
   case PCD_GT: binary->op = BOP_GT; break;
   case PCD_GE: binary->op = BOP_GTE; break;
   case PCD_LSHIFT: binary->op = BOP_SHIFT_L; break;
   case PCD_RSHIFT: binary->op = BOP_SHIFT_R; break;
   case PCD_ADD: binary->op = BOP_ADD; break;
   case PCD_SUBTRACT: binary->op = BOP_SUB; break;
   case PCD_MULTIPLY: binary->op = BOP_MUL; break;
   case PCD_DIVIDE: binary->op = BOP_DIV; break;
   case PCD_MODULUS: binary->op = BOP_MOD; break;
   default: break;
   }
   i32 precedence = get_binary_precedence( binary );
   binary->rside = pop( recovery, expr_recovery, precedence );
   binary->lside = pop( recovery, expr_recovery, precedence );
   push( expr_recovery, &binary->node, precedence );
   next_pcode( &expr_recovery->range );
}

static i32 get_binary_precedence( struct binary* binary ) {
   switch ( binary->op ) {
   case BOP_MUL:
   case BOP_DIV:
   case BOP_MOD:
      return PRECEDENCE_MUL;
   case BOP_ADD:
   case BOP_SUB:
      return PRECEDENCE_ADD;
   case BOP_SHIFT_L:
   case BOP_SHIFT_R:
      return PRECEDENCE_SHIFT;
   case BOP_LT:
   case BOP_LTE:
   case BOP_GT:
   case BOP_GTE:
      return PRECEDENCE_LT;
   case BOP_EQ:
   case BOP_NEQ:
      return PRECEDENCE_EQ;
   case BOP_BIT_AND:
      return PRECEDENCE_BITAND;
   case BOP_BIT_XOR:
      return PRECEDENCE_BITXOR;
   case BOP_BIT_OR:
      return PRECEDENCE_BITOR;
   case BOP_LOG_AND:
      return PRECEDENCE_LOGAND;
   case BOP_LOG_OR:
      return PRECEDENCE_LOGOR;
   default:
      return PRECEDENCE_TOP;
   }
}

static struct node* parenthesize_binary_operand( struct node* operand,
   i32 parent_precedence ) {
   if ( operand->type == NODE_BINARY ) {
      struct binary* binary = ( struct binary* ) operand;
      if ( get_binary_precedence( binary ) < parent_precedence ) {
         struct paren* paren = alloc_paren();
         paren->contents = operand;
         operand = &paren->node;
      }
   }
   return operand;
}

static struct paren* alloc_paren( void ) {
   struct paren* paren = mem_alloc( sizeof( *paren ) );
   paren->node.type = NODE_PAREN;
   paren->contents = NULL;
   return paren;
}

static void recover_unary( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->operand = pop( recovery, expr_recovery, PRECEDENCE_TOP );
   unary->op = UOP_MINUS;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_NEGATELOGICAL:
      unary->op = UOP_LOGICALNOT;
      break;
   case PCD_NEGATEBINARY:
      unary->op = UOP_BITWISENOT;
      break;
   default:
      break;
   }
   push( expr_recovery, &unary->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void recover_minus( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct unary* unary = mem_alloc( sizeof( *unary ) );
   unary->node.type = NODE_UNARY;
   unary->operand = pop( recovery, expr_recovery, PRECEDENCE_TOP );
   unary->op = UOP_MINUS;
   // Create separation between similar-looking tokens.
   bool parenthesize = ( ( unary->operand->type == NODE_UNARY &&
      ( ( struct unary* ) unary->operand )->op == UOP_MINUS ) ||
      ( unary->operand->type == NODE_INC &&
      ( ( struct inc* ) unary->operand )->decrement ) );
   if ( parenthesize ) {
      struct paren* paren = mem_alloc( sizeof( *paren ) );
      paren->node.type = NODE_PAREN;
      paren->contents = unary->operand;
      unary->operand = &paren->node;
   }
   push( expr_recovery, &unary->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void recover_inc( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct inc* inc = mem_alloc( sizeof( *inc ) );
   inc->node.type = NODE_INC;
   inc->operand = recover_var( recovery, expr_recovery );
   inc->decrement = false;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
      inc->decrement = true;
   }
   push( expr_recovery, &inc->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PUSHSCRIPTVAR:
   case PCD_PUSHMAPVAR:
   case PCD_PUSHWORLDVAR:
      next_pcode( &expr_recovery->range );
   }
}

static void recover_array_inc( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct inc* inc = mem_alloc( sizeof( *inc ) );
   inc->node.type = NODE_INC;
   inc->operand = NULL;
   inc->decrement = false;

   struct node* var = recover_var( recovery, expr_recovery );
   struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
   subscript->node.type = NODE_SUBSCRIPT;
   subscript->lside = var;
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   subscript->index = expr;
   inc->operand = &subscript->node;

   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_DECSCRIPTARRAY:
   case PCD_DECMAPARRAY:
   case PCD_DECWORLDARRAY:
      inc->decrement = true;
   }
   push( expr_recovery, &inc->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PUSHSCRIPTARRAY:
   case PCD_PUSHMAPARRAY:
   case PCD_PUSHWORLDARRAY:
      next_pcode( &expr_recovery->range );
   }
}

static void examine_pushvar( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct node* operand = recover_var( recovery, expr_recovery );
   next_pcode( &expr_recovery->range );
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
      recover_post_inc( expr_recovery, operand );
      break;
   default:
      push( expr_recovery, operand, PRECEDENCE_TOP );
   }
}

static void examine_pusharray( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct node* lside = recover_var( recovery, expr_recovery );
   struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
   subscript->node.type = NODE_SUBSCRIPT;
   subscript->lside = lside;
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   subscript->index = expr;
   push( expr_recovery, &subscript->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );

/*
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
      recover_post_inc( expr_recovery, operand );
      break;
   default:
      push( expr_recovery, operand, PRECEDENCE_TOP );
   }
*/
}

static void recover_post_inc( struct expr_recovery* expr_recovery,
   struct node* operand ) {
   struct inc* inc = mem_alloc( sizeof( *inc ) );
   inc->node.type = NODE_INCPOST;
   inc->operand = operand;
   inc->decrement = false;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
      inc->decrement = true;
   }
   push( expr_recovery, &inc->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void recover_assign( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct assign* assign = mem_alloc( sizeof( *assign ) );
   assign->node.type = NODE_ASSIGN;
   assign->lside = recover_var( recovery, expr_recovery );
   assign->rside = pop( recovery, expr_recovery, PRECEDENCE_ASSIGN );
   assign->op = AOP_SIMPLE;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_ADDSCRIPTVAR:
   case PCD_ADDMAPVAR:
   case PCD_ADDWORLDVAR:
      assign->op = AOP_ADD;
      break;
   case PCD_SUBSCRIPTVAR:
   case PCD_SUBMAPVAR:
   case PCD_SUBWORLDVAR:
      assign->op = AOP_SUB;
      break;
   case PCD_MULSCRIPTVAR:
   case PCD_MULMAPVAR:
   case PCD_MULWORLDVAR:
      assign->op = AOP_MUL;
      break;
   case PCD_DIVSCRIPTVAR:
   case PCD_DIVMAPVAR:
   case PCD_DIVWORLDVAR:
      assign->op = AOP_DIV;
      break;
   case PCD_MODSCRIPTVAR:
   case PCD_MODMAPVAR:
   case PCD_MODWORLDVAR:
      assign->op = AOP_MOD;
      break;
   case PCD_ANDSCRIPTVAR:
   case PCD_ANDMAPVAR:
   case PCD_ANDWORLDVAR:
      assign->op = AOP_BIT_AND;
      break;
   case PCD_EORSCRIPTVAR:
   case PCD_EORMAPVAR:
   case PCD_EORWORLDVAR:
      assign->op = AOP_BIT_XOR;
      break;
   case PCD_ORSCRIPTVAR:
   case PCD_ORMAPVAR:
   case PCD_ORWORLDVAR:
      assign->op = AOP_BIT_OR;
      break;
   case PCD_LSSCRIPTVAR:
   case PCD_LSMAPVAR:
   case PCD_LSWORLDVAR:
      assign->op = AOP_SHIFT_L;
      break;
   case PCD_RSSCRIPTVAR:
   case PCD_RSMAPVAR:
   case PCD_RSWORLDVAR:
      assign->op = AOP_SHIFT_R;
      break;
   default:
      break;
   }
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_ASSIGNMAPVAR:
     // var->storage = STORAGE_MAP;
      break;
   case PCD_ASSIGNWORLDVAR:
     // var->storage = STORAGE_WORLD;
      break;
   default:
      break;
   }
   push( expr_recovery, &assign->node, PRECEDENCE_ASSIGN );
   next_pcode( &expr_recovery->range );
}

static void recover_assign_array( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct assign* assign = mem_alloc( sizeof( *assign ) );
   assign->node.type = NODE_ASSIGN;
   assign->lside = NULL;
   assign->rside = pop( recovery, expr_recovery, PRECEDENCE_ASSIGN );
   assign->op = AOP_SIMPLE;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_ASSIGNSCRIPTARRAY:
   case PCD_ASSIGNMAPARRAY:
   case PCD_ASSIGNWORLDARRAY:
   case PCD_ASSIGNGLOBALARRAY:
      break;
   case PCD_ADDSCRIPTARRAY:
   case PCD_ADDMAPARRAY:
   case PCD_ADDWORLDARRAY:
   case PCD_ADDGLOBALARRAY:
      assign->op = AOP_ADD;
      break;
   case PCD_SUBSCRIPTARRAY:
   case PCD_SUBMAPARRAY:
   case PCD_SUBWORLDARRAY:
   case PCD_SUBGLOBALARRAY:
      assign->op = AOP_SUB;
      break;
   case PCD_MULSCRIPTARRAY:
   case PCD_MULMAPARRAY:
   case PCD_MULWORLDARRAY:
   case PCD_MULGLOBALARRAY:
      assign->op = AOP_MUL;
      break;
   case PCD_DIVSCRIPTARRAY:
   case PCD_DIVMAPARRAY:
   case PCD_DIVWORLDARRAY:
   case PCD_DIVGLOBALARRAY:
      assign->op = AOP_DIV;
      break;
   case PCD_MODSCRIPTARRAY:
   case PCD_MODMAPARRAY:
   case PCD_MODWORLDARRAY:
   case PCD_MODGLOBALARRAY:
      assign->op = AOP_MOD;
      break;
   case PCD_ANDSCRIPTARRAY:
   case PCD_ANDMAPARRAY:
   case PCD_ANDWORLDARRAY:
   case PCD_ANDGLOBALARRAY:
      assign->op = AOP_BIT_AND;
      break;
   case PCD_EORSCRIPTARRAY:
   case PCD_EORMAPARRAY:
   case PCD_EORWORLDARRAY:
   case PCD_EORGLOBALARRAY:
      assign->op = AOP_BIT_XOR;
      break;
   case PCD_ORSCRIPTARRAY:
   case PCD_ORMAPARRAY:
   case PCD_ORWORLDARRAY:
   case PCD_ORGLOBALARRAY:
      assign->op = AOP_BIT_OR;
      break;
   case PCD_LSSCRIPTARRAY:
   case PCD_LSMAPARRAY:
   case PCD_LSWORLDARRAY:
   case PCD_LSGLOBALARRAY:
      assign->op = AOP_SHIFT_L;
      break;
   case PCD_RSSCRIPTARRAY:
   case PCD_RSMAPARRAY:
   case PCD_RSWORLDARRAY:
   case PCD_RSGLOBALARRAY:
      assign->op = AOP_SHIFT_R;
      break;
   default:
      UNREACHABLE();
      t_bail( recovery->task );
   }
   struct node* var = recover_var( recovery, expr_recovery );
   struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
   subscript->node.type = NODE_SUBSCRIPT;
   subscript->lside = var;
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   subscript->index = expr;
   assign->lside = &subscript->node;
   push( expr_recovery, &assign->node, PRECEDENCE_ASSIGN );
   next_pcode( &expr_recovery->range );
}

static struct node* recover_var( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct var* var = NULL;
   u32 index = ( u32 ) expr_recovery->range.generic->args->value;
   struct var** list = NULL;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PUSHSCRIPTVAR:
   case PCD_ASSIGNSCRIPTVAR:
   case PCD_ADDSCRIPTVAR:
   case PCD_SUBSCRIPTVAR:
   case PCD_MULSCRIPTVAR:
   case PCD_DIVSCRIPTVAR:
   case PCD_MODSCRIPTVAR:
   case PCD_INCSCRIPTVAR:
   case PCD_DECSCRIPTVAR:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( recovery->script ) {
         if ( ! ( index < recovery->script->num_vars ) ) {
            t_diag( recovery->task, DIAG_ERR,
               "invalid script variable: %d", index );
            t_bail( recovery->task );
         }
         if ( ! recovery->script->vars[ index ] ) {
            var = alloc_var();
            var->index = index;
            recovery->script->vars[ index ] = var;
         }
         var = recovery->script->vars[ index ];
      }
      else {
         if ( ! ( index < recovery->func->more.user->num_vars ) ) {
            t_diag( recovery->task, DIAG_ERR,
               "invalid function variable: %d", index );
            t_bail( recovery->task );
         }
         if ( ! recovery->func->more.user->vars[ index ] ) {
            var = alloc_var();
            var->index = index;
            recovery->func->more.user->vars[ index ] = var;
         }
         var = recovery->func->more.user->vars[ index ];
      }
      break;
   case PCD_PUSHSCRIPTARRAY:
   case PCD_ASSIGNSCRIPTARRAY:
   case PCD_ADDSCRIPTARRAY:
   case PCD_SUBSCRIPTARRAY:
   case PCD_MULSCRIPTARRAY:
   case PCD_DIVSCRIPTARRAY:
   case PCD_MODSCRIPTARRAY:
   case PCD_ANDSCRIPTARRAY:
   case PCD_EORSCRIPTARRAY:
   case PCD_ORSCRIPTARRAY:
   case PCD_LSSCRIPTARRAY:
   case PCD_RSSCRIPTARRAY:
   case PCD_INCSCRIPTARRAY:
   case PCD_DECSCRIPTARRAY:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( recovery->script ) {
         if ( ! recovery->script->arrays[ index ] ) {
            var = alloc_var();
            var->index = index;
            recovery->script->arrays[ index ] = var;
         }
         var = recovery->script->arrays[ index ];
      }
      else {
         if ( ! recovery->func->more.user->arrays[ index ] ) {
            var = alloc_var();
            var->index = index;
            recovery->func->more.user->arrays[ index ] = var;
         }
         var = recovery->func->more.user->arrays[ index ];
      }
      break;
   case PCD_PUSHMAPVAR:
   case PCD_ASSIGNMAPVAR:
   case PCD_ADDMAPVAR:
   case PCD_SUBMAPVAR:
   case PCD_MULMAPVAR:
   case PCD_DIVMAPVAR:
   case PCD_MODMAPVAR:
   case PCD_INCMAPVAR:
   case PCD_DECMAPVAR:
   case PCD_PUSHMAPARRAY:
   case PCD_ASSIGNMAPARRAY:
   case PCD_ADDMAPARRAY:
   case PCD_SUBMAPARRAY:
   case PCD_MULMAPARRAY:
   case PCD_DIVMAPARRAY:
   case PCD_MODMAPARRAY:
   case PCD_ANDMAPARRAY:
   case PCD_EORMAPARRAY:
   case PCD_ORMAPARRAY:
   case PCD_LSMAPARRAY:
   case PCD_RSMAPARRAY:
   case PCD_INCMAPARRAY:
   case PCD_DECMAPARRAY:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( ! recovery->task->map_vars[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_MAP;
         var->index = index;
         recovery->task->map_vars[ index ] = var;
      }
      var = recovery->task->map_vars[ index ];
      break;
   case PCD_PUSHWORLDVAR:
   case PCD_ASSIGNWORLDVAR:
   case PCD_ADDWORLDVAR:
   case PCD_SUBWORLDVAR:
   case PCD_MULWORLDVAR:
   case PCD_DIVWORLDVAR:
   case PCD_MODWORLDVAR:
   case PCD_INCWORLDVAR:
   case PCD_DECWORLDVAR:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( ! recovery->task->world_vars[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_WORLD;
         var->index = index;
         recovery->task->world_vars[ index ] = var;
      }
      var = recovery->task->world_vars[ index ];
      break;
   case PCD_PUSHWORLDARRAY:
   case PCD_ASSIGNWORLDARRAY:
   case PCD_ADDWORLDARRAY:
   case PCD_SUBWORLDARRAY:
   case PCD_MULWORLDARRAY:
   case PCD_DIVWORLDARRAY:
   case PCD_MODWORLDARRAY:
   case PCD_ANDWORLDARRAY:
   case PCD_EORWORLDARRAY:
   case PCD_ORWORLDARRAY:
   case PCD_LSWORLDARRAY:
   case PCD_RSWORLDARRAY:
   case PCD_INCWORLDARRAY:
   case PCD_DECWORLDARRAY:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( ! recovery->task->world_arrays[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_WORLD;
         var->index = index;
         var->array = true;
         recovery->task->world_arrays[ index ] = var;
      }
      var = recovery->task->world_arrays[ index ];
      break;
   case PCD_PUSHGLOBALVAR:
   case PCD_ASSIGNGLOBALVAR:
   case PCD_ADDGLOBALVAR:
   case PCD_SUBGLOBALVAR:
   case PCD_MULGLOBALVAR:
   case PCD_DIVGLOBALVAR:
   case PCD_MODGLOBALVAR:
   case PCD_INCGLOBALVAR:
   case PCD_DECGLOBALVAR:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( ! recovery->task->global_vars[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_GLOBAL;
         var->index = index;
         recovery->task->global_vars[ index ] = var;
      }
      var = recovery->task->global_vars[ index ];
      break;
   case PCD_PUSHGLOBALARRAY:
   case PCD_ASSIGNGLOBALARRAY:
   case PCD_ADDGLOBALARRAY:
   case PCD_SUBGLOBALARRAY:
   case PCD_MULGLOBALARRAY:
   case PCD_DIVGLOBALARRAY:
   case PCD_MODGLOBALARRAY:
   case PCD_ANDGLOBALARRAY:
   case PCD_EORGLOBALARRAY:
   case PCD_ORGLOBALARRAY:
   case PCD_LSGLOBALARRAY:
   case PCD_RSGLOBALARRAY:
   case PCD_INCGLOBALARRAY:
   case PCD_DECGLOBALARRAY:
      index = ( u32 ) expr_recovery->range.generic->args->value;
      if ( ! recovery->task->global_arrays[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_GLOBAL;
         var->index = index;
         var->array = true;
         recovery->task->global_arrays[ index ] = var;
      }
      var = recovery->task->global_arrays[ index ];
      break;
   default:
printf( "%d\n", expr_recovery->range.pcode->opcode );
      UNREACHABLE();
      t_bail( recovery->task );
   }
   return ( struct node* ) var;
}

struct var* t_reserve_map_var( struct task* task, u32 index ) {
   if ( ! task->map_vars[ index ] ) {
      struct var* var = alloc_var();
      var->storage = STORAGE_MAP;
      var->index = index;
      task->map_vars[ index ] = var;
   }
   return task->map_vars[ index ];
}

static struct var* alloc_script_var( struct recovery* recovery, u32 index ) {
   struct var* var = alloc_var();
   var->index = index;
   recovery->script->vars[ index ] = var;
   return var;
}

static struct var* alloc_var( void ) {
   struct var* var = mem_alloc( sizeof( *var ) );
   var->node.type = NODE_VAR;
   str_init( &var->name );
   var->initz = NULL;
   var->value = NULL;
   var->storage = STORAGE_LOCAL;
   var->dim_length = 0;
   var->index = 0;
   var->spec = SPEC_INT;
   var->array = false;
   var->imported = false;
   var->used = false;
   var->declared = false;
   return var;
}

struct var* t_alloc_var( void ) {
   return alloc_var();
}

static void recover_call_aspec( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   #define ENTRY( name ) { { NODE_ASPEC }, name }
   static struct aspec table[] = {
      // 0
      ENTRY( "" ),
      ENTRY( "Polyobj_StartLine" ),
      ENTRY( "Polyobj_RotateLeft" ),
      ENTRY( "Polyobj_RotateRight" ),
      ENTRY( "Polyobj_Move" ),
      ENTRY( "Polyobj_ExplicitLine" ),
      ENTRY( "Polyobj_MoveTimes8" ),
      ENTRY( "Polyobj_DoorSwing" ),
      ENTRY( "Polyobj_DoorSlide" ),
      ENTRY( "Line_Horizon" ),
      // 10
      ENTRY( "Door_Close" ),
      ENTRY( "Door_Open" ),
      ENTRY( "Door_Raise" ),
      ENTRY( "Door_LockedRaise" ),
      ENTRY( "Door_Animated" ),
      ENTRY( "Autosave" ),
      ENTRY( "Transfer_WallLight" ),
      ENTRY( "Thing_Raise" ),
      ENTRY( "StartConversation" ),
      ENTRY( "Thing_Stop" ),
      // 20
      ENTRY( "Floor_LowerByValue" ),
      ENTRY( "Floor_LowerToLowest" ),
      ENTRY( "Floor_LowerToNearest" ),
      ENTRY( "Floor_RaiseByValue" ),
      ENTRY( "Floor_RaiseToHighest" ),
      ENTRY( "Floor_RaiseToNearest" ),
      ENTRY( "Stairs_BuildDown" ),
      ENTRY( "Stairs_BuildUp" ),
      ENTRY( "Floor_RaiseAndCrush" ),
      ENTRY( "Pillar_Build" ),
      // 30
      ENTRY( "Pillar_Open" ),
      ENTRY( "Stairs_BuildDownSync" ),
      ENTRY( "Stairs_BuildUpSync" ),
      ENTRY( "ForceField" ),
      ENTRY( "ClearForceField" ),
      ENTRY( "Floor_RaiseByValueTimes8" ),
      ENTRY( "Floor_LowerByValueTimes8" ),
      ENTRY( "Floor_MoveToValue" ),
      ENTRY( "Ceiling_Waggle" ),
      ENTRY( "Teleport_ZombieChanger" ),
      // 40
      ENTRY( "Ceiling_LowerByValue" ),
      ENTRY( "Ceiling_RaiseByValue" ),
      ENTRY( "Ceiling_CrushAndRaise" ),
      ENTRY( "Ceiling_LowerAndCrush" ),
      ENTRY( "Ceiling_CrushStop" ),
      ENTRY( "Ceiling_CrushRaiseAndStay" ),
      ENTRY( "Floor_CrushStop" ),
      ENTRY( "Ceiling_MoveToValue" ),
      ENTRY( "Sector_Attach3dMidTex" ),
      ENTRY( "GlassBreak" ),
      // 50
      ENTRY( "ExtraFloor_LightOnly" ),
      ENTRY( "Sector_SetLink" ),
      ENTRY( "Scroll_Wall" ),
      ENTRY( "Line_SetTextureOffset" ),
      ENTRY( "Sector_ChangeFlags" ),
      ENTRY( "Line_SetBlocking" ),
      ENTRY( "Line_SetTextureScale" ),
      ENTRY( "Sector_SetPortal" ),
      ENTRY( "Sector_CopyScroller" ),
      ENTRY( "Polyobj_Or_MoveToSpot" ),
      // 60
      ENTRY( "Plat_PerpetualRaise" ),
      ENTRY( "Plat_Stop" ),
      ENTRY( "Plat_DownWaitUpStay" ),
      ENTRY( "Plat_DownByValue" ),
      ENTRY( "Plat_UpWaitDownStay" ),
      ENTRY( "Plat_UpByValue" ),
      ENTRY( "Floor_LowerInstant" ),
      ENTRY( "Floor_RaiseInstant" ),
      ENTRY( "Floor_MoveToValueTimes8" ),
      ENTRY( "Ceiling_MoveToValueTimes8" ),
      // 70
      ENTRY( "Teleport" ),
      ENTRY( "Teleport_NoFog" ),
      ENTRY( "ThrustThing" ),
      ENTRY( "DamageThing" ),
      ENTRY( "Teleport_NewMap" ),
      ENTRY( "Teleport_EndGame" ),
      ENTRY( "TeleportOther" ),
      ENTRY( "TeleportGroup" ),
      ENTRY( "TeleportInSector" ),
      ENTRY( "Thing_SetConversation" ),
      // 80
      ENTRY( "Acs_Execute" ),
      ENTRY( "Acs_Suspend" ),
      ENTRY( "Acs_Terminate" ),
      ENTRY( "Acs_LockedExecute" ),
      ENTRY( "Acs_ExecuteWithResult" ),
      ENTRY( "Acs_LockedExecuteDoor" ),
      ENTRY( "Polyobj_MoveToSpot" ),
      ENTRY( "Polyobj_Stop" ),
      ENTRY( "Polyobj_MoveTo" ),
      ENTRY( "Polyobj_Or_MoveTo" ),
      // 90
      ENTRY( "Polyobj_Or_RotateLeft" ),
      ENTRY( "Polyobj_Or_RotateRight" ),
      ENTRY( "Polyobj_Or_Move" ),
      ENTRY( "Polyobj_Or_MoveTimes8" ),
      ENTRY( "Pillar_BuildAndCrush" ),
      ENTRY( "FloorAndCeiling_LowerByValue" ),
      ENTRY( "FloorAndCeiling_RaiseByValue" ),
      ENTRY( "Ceiling_LowerAndCrushDist" ),
      ENTRY( "Sector_SetTranslucent" ),
      ENTRY( "Floor_RaiseAndCrushDoom" ),
      // 100
      ENTRY( "Scroll_Texture_Left" ),
      ENTRY( "Scroll_Texture_Right" ),
      ENTRY( "Scroll_Texture_Up" ),
      ENTRY( "Scroll_Texture_Down" ),
      ENTRY( "Ceiling_CrushAndRaiseSilentDist" ),
      ENTRY( "Door_WaitRaise" ),
      ENTRY( "Door_WaitClose" ),
      ENTRY( "Line_SetPortalTarget" ),
      ENTRY( "" ),
      ENTRY( "Light_ForceLightning" ),
      // 110
      ENTRY( "Light_RaiseByValue" ),
      ENTRY( "Light_LowerByValue" ),
      ENTRY( "Light_ChangeToValue" ),
      ENTRY( "Light_Fade" ),
      ENTRY( "Light_Glow" ),
      ENTRY( "Light_Flicker" ),
      ENTRY( "Light_Strobe" ),
      ENTRY( "Light_Stop" ),
      ENTRY( "Plane_Copy" ),
      ENTRY( "Thing_Damage" ),
      // 120
      ENTRY( "Radius_Quake" ),
      ENTRY( "Line_SetIdentification" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "Thing_Move" ),
      ENTRY( "" ),
      ENTRY( "Thing_SetSpecial" ),
      ENTRY( "ThrustThingZ" ),
      ENTRY( "UsePuzzleItem" ),
      // 130
      ENTRY( "Thing_Activate" ),
      ENTRY( "Thing_Deactivate" ),
      ENTRY( "Thing_Remove" ),
      ENTRY( "Thing_Destroy" ),
      ENTRY( "Thing_Projectile" ),
      ENTRY( "Thing_Spawn" ),
      ENTRY( "Thing_ProjectileGravity" ),
      ENTRY( "Thing_SpawnNoFog" ),
      ENTRY( "Floor_Waggle" ),
      ENTRY( "Thing_SpawnFacing" ),
      // 140
      ENTRY( "Sector_ChangeSound" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "Player_SetTeam" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      // 150
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "Team_Score" ),
      ENTRY( "Team_GivePoints" ),
      ENTRY( "Teleport_NoStop" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "SetGlobalFogParameter" ),
      ENTRY( "Fs_Excute" ),
      ENTRY( "Sector_SetPlaneReflection" ),
      // 160
      ENTRY( "Sector_Set3dFloor" ),
      ENTRY( "Sector_SetContents" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "" ),
      ENTRY( "Ceiling_CrushAndRaiseDist" ),
      ENTRY( "Generic_Crusher2" ),
      // 170
      ENTRY( "Sector_SetCeilingScale2" ),
      ENTRY( "Sector_SetFloorScale2" ),
      ENTRY( "Plat_UpNearestWaitDownStay" ),
      ENTRY( "NoiseAlert" ),
      ENTRY( "SendToCommunicator" ),
      ENTRY( "Thing_ProjectileIntercept" ),
      ENTRY( "Thing_ChangeTid" ),
      ENTRY( "Thing_Hate" ),
      ENTRY( "Thing_ProjectileAimed" ),
      ENTRY( "ChangeSkill" ),
      // 180
      ENTRY( "Thing_SetTranslation" ),
      ENTRY( "Plane_Align" ),
      ENTRY( "Line_Mirror" ),
      ENTRY( "Line_AlignCeiling" ),
      ENTRY( "Line_AlignFloor" ),
      ENTRY( "Sector_SetRotation" ),
      ENTRY( "Sector_SetCeilingPanning" ),
      ENTRY( "Sector_SetFloorPanning" ),
      ENTRY( "Sector_SetCeilingScale" ),
      ENTRY( "Sector_SetFloorScale" ),
      // 190
      ENTRY( "Static_Init" ),
      ENTRY( "SetPlayerProperty" ),
      ENTRY( "Ceiling_LowerToHighestFloor" ),
      ENTRY( "Ceiling_LowerInstant" ),
      ENTRY( "Ceiling_RaiseInstant" ),
      ENTRY( "Ceiling_CrushRaiseAndStayA" ),
      ENTRY( "Ceiling_CrushAndRaiseA" ),
      ENTRY( "Ceiling_CrushAndRaiseSilentA" ),
      ENTRY( "Ceiling_RaiseByValueTimes8" ),
      ENTRY( "Ceiling_LowerByValueTimes8" ),
      // 200
      ENTRY( "Generic_Floor" ),
      ENTRY( "Generic_Ceiling" ),
      ENTRY( "Generic_Door" ),
      ENTRY( "Generic_Lift" ),
      ENTRY( "Generic_Stairs" ),
      ENTRY( "Generic_Crusher" ),
      ENTRY( "Plat_DownWaitUpStayLip" ),
      ENTRY( "Plat_PerpetualRaiseLip" ),
      ENTRY( "TranslucentLine" ),
      ENTRY( "Transfer_Heights" ),
      // 210
      ENTRY( "Transfer_FloorLight" ),
      ENTRY( "Transfer_CeilingLight" ),
      ENTRY( "Sector_SetColor" ),
      ENTRY( "Sector_SetFade" ),
      ENTRY( "Sector_SetDamage" ),
      ENTRY( "Teleport_Line" ),
      ENTRY( "Sector_SetGravity" ),
      ENTRY( "Stairs_BuildUpDoom" ),
      ENTRY( "Sector_SetWind" ),
      ENTRY( "Sector_SetFriction" ),
      // 220
      ENTRY( "Sector_SetCurrent" ),
      ENTRY( "Scroll_Texture_Both" ),
      ENTRY( "Scroll_Texture_Model" ),
      ENTRY( "Scroll_Floor" ),
      ENTRY( "Scroll_Ceiling" ),
      ENTRY( "Scroll_Texture_Offsets" ),
      ENTRY( "Acs_ExecuteAlways" ),
      ENTRY( "PointPush_SetForce" ),
      ENTRY( "Plat_RaiseAndStayTx0" ),
      ENTRY( "Thing_SetGoal" ),
      // 230
      ENTRY( "Plat_UpByValueStayTx" ),
      ENTRY( "Plat_ToggleCeiling" ),
      ENTRY( "Light_StrobeDoom" ),
      ENTRY( "Light_MinNeighbor" ),
      ENTRY( "Light_MaxNeighbor" ),
      ENTRY( "Floor_TransferTrigger" ),
      ENTRY( "Floor_TransferNumeric" ),
      ENTRY( "ChangeCamera" ),
      ENTRY( "Floor_RaiseToLowestCeiling" ),
      ENTRY( "Floor_RaiseByValueTxTy" ),
      // 240
      ENTRY( "Floor_RaiseByTexture" ),
      ENTRY( "Floor_LowerToLowestTxTy" ),
      ENTRY( "Floor_LowerToHighest" ),
      ENTRY( "Exit_Normal" ),
      ENTRY( "Exit_Secret" ),
      ENTRY( "Elevator_RaiseToNearest" ),
      ENTRY( "Elevator_MoveToFloor" ),
      ENTRY( "Elevator_LowerToNearest" ),
      ENTRY( "HealThing" ),
      ENTRY( "Door_CloseWaitOpen" ),
      // 250
      ENTRY( "Floor_Donut" ),
      ENTRY( "FloorAndCeiling_LowerRaise" ),
      ENTRY( "Ceiling_RaiseToNearest" ),
      ENTRY( "Ceiling_LowerToLowest" ),
      ENTRY( "Ceiling_LowerToFloor" ),
      ENTRY( "Ceiling_CrushRaiseAndStaySilA" ),
      ENTRY( "Floor_LowerToHighestEE" ),
      ENTRY( "Floor_RaiseToLowest" ),
      ENTRY( "Floor_LowerToLowestCeiling" ),
      ENTRY( "Floor_RaiseToCeiling" ),
      // 260
      ENTRY( "Floor_ToCeilingInstant" ),
      ENTRY( "Floor_LowerByTexture" ),
      ENTRY( "Ceiling_RaiseToHighest" ),
      ENTRY( "Ceiling_ToHighestInstant" ),
      ENTRY( "Ceiling_LowerToNearest" ),
      ENTRY( "Ceiling_RaiseToLowest" ),
      ENTRY( "Ceiling_RaiseToHighestFloor" ),
      ENTRY( "Ceiling_ToFloorInstant" ),
      ENTRY( "Ceiling_RaiseByTexture" ),
      ENTRY( "Ceiling_LowerByTexture" ),
      // 270
      ENTRY( "Stairs_BuildDownDoom" ),
      ENTRY( "Stairs_BuildUpDoomSync" ),
      ENTRY( "Stairs_BuildDownDoomSync" ),
      ENTRY( "Stairs_BuildUpDoomCrush" ),
      ENTRY( "Door_AnimatedClose" ),
      ENTRY( "Floor_Stop" ),
      ENTRY( "Ceiling_Stop" ),
      ENTRY( "Sector_SetFloorGlow" ),
      ENTRY( "Sector_SetCeilingGlow" ),
   };
   #undef ENTRY
   if ( expr_recovery->range.pcode->note &&
      expr_recovery->range.pcode->note->type == NOTE_INTERNFUNC ) {
      recover_call_intern( recovery, expr_recovery );
      return;
   }
   struct call* call = alloc_call();
   call->operand = &table[ 0 ].node;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_LSPEC1DIRECT:
   case PCD_LSPEC2DIRECT:
   case PCD_LSPEC3DIRECT:
   case PCD_LSPEC4DIRECT:
   case PCD_LSPEC5DIRECT:
   case PCD_LSPEC1DIRECTB:
   case PCD_LSPEC2DIRECTB:
   case PCD_LSPEC3DIRECTB:
   case PCD_LSPEC4DIRECTB:
   case PCD_LSPEC5DIRECTB:
      call->direct = true;
      break;
   }
   i32 id = expr_recovery->range.generic->args->value;
   if ( id >= 0 && ( u32 ) id < ARRAY_SIZE( table ) ) {
      call->operand = &table[ id ].node;
   }
   else {
      struct unknown* unknown = alloc_unknown();
      unknown->type = UNKNOWN_ASPEC;
      unknown->more.aspec.id = id;
      call->operand = &unknown->node;
   }
   if ( call->direct ) {
      struct generic_pcode_arg* arg = expr_recovery->range.generic->args->next;
      while ( arg ) {
         struct literal* literal = alloc_literal();
         literal->value = arg->value;
         struct expr* expr = alloc_expr();
         expr->root = &literal->node;
         list_append( &call->args, expr );
         arg = arg->next;
      }
   }
   else {
      i32 num_args = 0;
      switch ( expr_recovery->range.pcode->opcode ) {
      case PCD_LSPEC1:
      case PCD_LSPEC2:
      case PCD_LSPEC3:
      case PCD_LSPEC4:
      case PCD_LSPEC5:
         num_args = expr_recovery->range.pcode->opcode - PCD_LSPEC1 + 1;
         break;
      case PCD_LSPEC5RESULT:
      case PCD_LSPEC5EX:
      case PCD_LSPEC5EXRESULT:
         num_args = 5;
         break;
      default:
         UNREACHABLE();
         t_bail( recovery->task );
      }
      for ( i32 i = 0; i < num_args; ++i ) {
         struct expr* expr = alloc_expr();
         expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
         list_prepend( &call->args, expr );
      }
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
   recovery->task->calls_aspec = true;
}

struct expr* t_alloc_literal_expr( i32 value ) {
   struct literal* literal = alloc_literal();
   literal->value = value;
   struct expr* expr = alloc_expr();
   expr->root = &literal->node;
   return expr;
}

static struct unknown* alloc_unknown( void ) {
   struct unknown* unknown = mem_alloc( sizeof( *unknown ) );
   unknown->node.type = NODE_UNKNOWN;
   unknown->type = UNKNOWN_ASPEC;
   return unknown;
}

static void recover_call_ext( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   if ( expr_recovery->range.pcode->note &&
      expr_recovery->range.pcode->note->type == NOTE_INTERNFUNC ) {
      recover_call_intern( recovery, expr_recovery );
      return;
   }
   struct call* call = alloc_call();
   struct func* func = t_find_ext_func( recovery->task,
      expr_recovery->range.generic->args->next->value );
   if ( func ) {
      call->operand = &func->node;
   }
   else {
      struct unknown* unknown = alloc_unknown();
      unknown->type = UNKNOWN_EXT;
      unknown->more.ext.id = expr_recovery->range.generic->args->next->value;
      call->operand = &unknown->node;
   }
   for ( i32 i = 0; i < expr_recovery->range.generic->args->value; ++i ) {
      struct expr* expr = alloc_expr();
      expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
      list_prepend( &call->args, expr );
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
   recovery->task->calls_ext = true;
}

static void recover_call_intern( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct intern_func_note* note = ( struct intern_func_note* )
      expr_recovery->range.pcode->note;
   struct call* call = alloc_call();
   call->operand = &note->func->node;
   switch ( note->func->more.intern->id ) {
   case INTERNFUNC_ACSEXECUTEWAIT:
   case INTERNFUNC_ACSNAMEDEXECUTEWAIT:
      list_prepend( &call->args, pop_expr( recovery, expr_recovery ) );
      list_prepend( &call->args, pop_expr( recovery, expr_recovery ) );
      break;
   default:
      UNREACHABLE();
      t_bail( recovery->task );
   }
   pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   seek_pcode( &expr_recovery->range, note->exit );
printf( "%d\n", expr_recovery->stack.size );
}

static void recover_call_ded( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct func* func = t_get_ded_func( recovery->task,
      expr_recovery->range.pcode->opcode );
   struct call* call = alloc_call();
   call->operand = &func->node;
   for ( i32 i = 0; i < func->max_param; ++i ) {
      struct expr* expr = alloc_expr();
      expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
      list_prepend( &call->args, expr );
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void recover_call_ded_direct( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct func* func = t_get_ded_func( recovery->task,
      expr_recovery->range.pcode->opcode );
   struct call* call = alloc_call();
   call->operand = &func->node;
   call->direct = true;
   struct generic_pcode_arg* arg = expr_recovery->range.generic->args;
   while ( arg ) {
      struct literal* literal = alloc_literal();
      literal->value = arg->value;
      struct expr* expr = alloc_expr();
      expr->root = &literal->node;
      list_append( &call->args, expr );
      arg = arg->next;
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static struct call* alloc_call( void ) {
   struct call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_CALL;
   call->operand = NULL;
   call->format_item = NULL;
   list_init( &call->args );
   call->direct = false;
   return call;
}

static void examine_beginprint( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct format_call_recovery* parent = expr_recovery->format_call;
   struct format_call_recovery call_recovery;
   call_recovery.func = NULL;
   call_recovery.format_item = NULL;
   call_recovery.format_item_tail = NULL;
   call_recovery.args_start = 0;
   expr_recovery->format_call = &call_recovery;
   next_pcode( &expr_recovery->range );
   examine_expr( recovery, expr_recovery );
   struct call* call = alloc_call();
   call->operand = &call_recovery.func->node;
   call->format_item = call_recovery.format_item;
   u32 num_regular_args = list_size( &expr_recovery->stack ) -
      call_recovery.args_start;
   for ( u32 i = 0; i < num_regular_args; ++i ) {
      struct expr* expr = alloc_expr();
      expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
      list_prepend( &call->args, expr );
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   expr_recovery->format_call = parent;
}

static void examine_printvalue( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct format_item* item = alloc_format_item();
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   item->value = expr;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PRINTNUMBER: break;
   case PCD_PRINTSTRING: item->cast = FCAST_STRING; break;
   case PCD_PRINTCHARACTER: item->cast = FCAST_CHAR; break;
   case PCD_PRINTFIXED: item->cast = FCAST_FIXED; break;
   case PCD_PRINTNAME: item->cast = FCAST_NAME; break;
   case PCD_PRINTLOCALIZED: item->cast = FCAST_LOCAL_STRING; break;
   case PCD_PRINTBIND: item->cast = FCAST_KEY; break;
   case PCD_PRINTBINARY: item->cast = FCAST_BINARY; break;
   case PCD_PRINTHEX: item->cast = FCAST_HEX; break;
   default:
      UNREACHABLE();
      t_bail( recovery->task );
   }
   append_format_item( expr_recovery->format_call, item );
   next_pcode( &expr_recovery->range );
}

static struct format_item* alloc_format_item( void ) {
   struct format_item* item = mem_alloc( sizeof( *item ) );
   item->cast = FCAST_DECIMAL;
   item->next = NULL;
   item->value = NULL;
   item->extra = NULL;
   return item;
}

static void append_format_item( struct format_call_recovery* recovery,
   struct format_item* item ) {
   if ( recovery->format_item ) {
      recovery->format_item_tail->next = item;
   }
   else {
      recovery->format_item = item;
   }
   recovery->format_item_tail = item;
}

static void examine_printarray( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct node* offset = NULL;
   struct node* length = NULL;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PRINTMAPCHRANGE:
   case PCD_PRINTWORLDCHRANGE:
   case PCD_PRINTGLOBALCHRANGE:
      length = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
      offset = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
      break;
   default:
      break;
   }
   struct node* node = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   struct node* sub_idx = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   if ( node->type != NODE_LITERAL ) {
      t_diag( recovery->task, DIAG_ERR,
         "printarray argument not a literal" );
      t_bail( recovery->task );
   }
   struct literal* literal = ( struct literal* ) node;
   u32 index = ( u32 ) literal->value;
   struct var* var = NULL;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_PRINTMAPCHARARRAY:
   case PCD_PRINTMAPCHRANGE:
      if ( ! recovery->task->map_vars[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_MAP;
         var->index = index;
         recovery->task->map_vars[ index ] = var;
      }
      var = recovery->task->map_vars[ index ];
      break;
   case PCD_PRINTWORLDCHARARRAY:
   case PCD_PRINTWORLDCHRANGE:
      if ( ! recovery->task->world_arrays[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_WORLD;
         var->index = index;
         var->array = true;
         recovery->task->world_arrays[ index ] = var;
      }
      var = recovery->task->world_arrays[ index ];
      break;
   case PCD_PRINTGLOBALCHARARRAY:
   case PCD_PRINTGLOBALCHRANGE:
      if ( ! recovery->task->global_arrays[ index ] ) {
         var = alloc_var();
         var->storage = STORAGE_GLOBAL;
         var->index = index;
         var->array = true;
         recovery->task->global_arrays[ index ] = var;
      }
      var = recovery->task->global_arrays[ index ];
      break;
   default:
printf( "%d\n", expr_recovery->range.pcode->opcode );
      UNREACHABLE();
      t_bail( recovery->task );
   }
   struct node* root = &var->node;
   if ( ! ( sub_idx->type == NODE_LITERAL &&
      ( ( struct literal* ) sub_idx )->value == 0 ) ) {
      struct subscript* subscript = mem_alloc( sizeof( *subscript ) );
      subscript->node.type = NODE_SUBSCRIPT;
      subscript->lside = &var->node;
      struct expr* expr = alloc_expr();
      expr->root = sub_idx;
      subscript->index = expr;
      root = &subscript->node;
   }
   struct format_item* item = alloc_format_item();
   struct expr* expr = alloc_expr();
   expr->root = root;
   item->cast = FCAST_ARRAY;
   item->value = expr;
   if ( offset ) {
      struct format_item_array* extra = mem_alloc( sizeof( *extra ) );
      struct expr* expr = alloc_expr();
      expr->root = offset;
      extra->offset = expr;
      expr = alloc_expr();
      expr->root = length;
      extra->length = expr;
      item->extra = extra;
   }
   append_format_item( expr_recovery->format_call, item );
   next_pcode( &expr_recovery->range );
   mem_free( literal );
}

static void examine_endprint( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct func* func = t_find_format_func( recovery->task,
      expr_recovery->range.pcode->opcode );
   if ( ! func ) {
      UNREACHABLE();
      t_bail( recovery->task );
   }
   expr_recovery->format_call->func = func;
   expr_recovery->done = true;
   next_pcode( &expr_recovery->range );
}

static void examine_strcpy( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct strcpy_call* call = mem_alloc( sizeof( *call ) );
   call->node.type = NODE_STRCPYCALL;
   call->offset = pop_expr( recovery, expr_recovery );
   call->string = pop_expr( recovery, expr_recovery );
   call->array_length = pop_expr( recovery, expr_recovery );
   call->array_offset = pop_expr( recovery, expr_recovery );
   call->array = pop_expr( recovery, expr_recovery );
   pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void examine_starttranslation( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct paltrans* trans = mem_alloc( sizeof( *trans ) );
   trans->node.type = NODE_PALTRANS;
   trans->number = NULL;
   list_init( &trans->ranges );
   trans->number = pop_expr( recovery, expr_recovery );
   next_pcode( &expr_recovery->range );
   expr_recovery->trans = trans;
}

static struct expr* pop_expr( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct expr* expr = alloc_expr();
   expr->root = pop( recovery, expr_recovery, PRECEDENCE_BOTTOM );
   return expr;
}

static void examine_translationrange( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct palrange* range = mem_alloc( sizeof( *range ) );
   range->type = PALRANGE_COLON;
   switch ( expr_recovery->range.pcode->opcode ) {
   case PCD_TRANSLATIONRANGE1:
      range->value.colon.end = pop_expr( recovery, expr_recovery );
      range->value.colon.begin = pop_expr( recovery, expr_recovery );
      break;
   case PCD_TRANSLATIONRANGE2:
   case PCD_TRANSLATIONRANGE3:
      range->value.rgb.blue2 = pop_expr( recovery, expr_recovery );
      range->value.rgb.green2 = pop_expr( recovery, expr_recovery );
      range->value.rgb.red2 = pop_expr( recovery, expr_recovery );
      range->value.rgb.blue1 = pop_expr( recovery, expr_recovery );
      range->value.rgb.green1 = pop_expr( recovery, expr_recovery );
      range->value.rgb.red1 = pop_expr( recovery, expr_recovery );
      range->type = ( expr_recovery->range.pcode->opcode ==
         PCD_TRANSLATIONRANGE3 ) ? PALRANGE_SATURATED : PALRANGE_RGB;
      break;
   case PCD_TRANSLATIONRANGE4:
      range->value.colorisation.blue = pop_expr( recovery, expr_recovery );
      range->value.colorisation.green = pop_expr( recovery, expr_recovery );
      range->value.colorisation.red = pop_expr( recovery, expr_recovery );
      range->type = PALRANGE_COLORISATION;
      break;
   case PCD_TRANSLATIONRANGE5:
      range->value.tint.blue = pop_expr( recovery, expr_recovery );
      range->value.tint.green = pop_expr( recovery, expr_recovery );
      range->value.tint.red = pop_expr( recovery, expr_recovery );
      range->value.tint.amount = pop_expr( recovery, expr_recovery );
      range->type = PALRANGE_TINT;
      break;
   default:
      UNREACHABLE();
      t_bail( recovery->task );
   }

/*
      c_push_expr( codegen, range->begin );
      c_push_expr( codegen, range->end );
      if ( range->rgb || range->saturated ) {
         c_push_expr( codegen, range->value.rgb.red1 );
         c_push_expr( codegen, range->value.rgb.green1 );
         c_push_expr( codegen, range->value.rgb.blue1 );
         c_push_expr( codegen, range->value.rgb.red2 );
         c_push_expr( codegen, range->value.rgb.green2 );
         c_push_expr( codegen, range->value.rgb.blue2 );
         if ( range->saturated ) {
            c_pcd( codegen, PCD_TRANSLATIONRANGE3 );
         }
         else {
            c_pcd( codegen, PCD_TRANSLATIONRANGE2 );
         }
      }
      else if ( range->colorisation ) {
         write_palrange_colorisation( codegen, range );
      }
      else if ( range->tint ) {
         write_palrange_tint( codegen, range );
      }
      else {
         c_push_expr( codegen, range->value.ent.begin );
         c_push_expr( codegen, range->value.ent.end );
         c_pcd( codegen, PCD_TRANSLATIONRANGE1 );
      }
      range = range->next;
*/
   range->end = pop_expr( recovery, expr_recovery );
   range->begin = pop_expr( recovery, expr_recovery );
   list_append( &expr_recovery->trans->ranges, range );
   next_pcode( &expr_recovery->range );
}

static void examine_endtranslation( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   push( expr_recovery, &expr_recovery->trans->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
   expr_recovery->done = true;
}

static void recover_call_user( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct func* func = t_find_func( recovery->task,
      ( u32 ) expr_recovery->range.generic->args->value );
   struct call* call = alloc_call();
   call->operand = &func->node;
   for ( i32 i = 0; i < func->max_param; ++i ) {
      struct expr* expr = alloc_expr();
      expr->root = pop( recovery, expr_recovery, PRECEDENCE_TOP );
      list_prepend( &call->args, expr );
   }
   push( expr_recovery, &call->node, PRECEDENCE_TOP );
   next_pcode( &expr_recovery->range );
}

static void recover_literal( struct expr_recovery* recovery ) {
   struct generic_pcode_arg* arg = recovery->range.generic->args;
   // Skip argument-count argument.
   if ( recovery->range.pcode->opcode == PCD_PUSHBYTES ) {
      arg = arg->next;
   }
   while ( arg ) {
      struct literal* literal = alloc_literal();
      literal->value = arg->value;
      push( recovery, &literal->node, PRECEDENCE_TOP );
      arg = arg->next;
   }
   next_pcode( &recovery->range );
}

static struct literal* alloc_literal( void ) {
   struct literal* literal = mem_alloc( sizeof( *literal ) );
   literal->node.type = NODE_LITERAL;
   literal->value = 0;
   return literal;
}

static void examine_dup( struct recovery* recovery,
   struct expr_recovery* expr_recovery ) {
   struct operand* operand = list_head( &expr_recovery->stack );
printf( "%p\n", ( void* ) operand );
   push( expr_recovery, operand->node, operand->precedence );
   next_pcode( &expr_recovery->range );
}

static void recover_expr_stmt( struct recovery* recovery,
   struct stmt_recovery* stmt_recovery ) {
   struct expr_stmt_note* note = ( struct expr_stmt_note* )
      stmt_recovery->range.pcode->note;
   struct expr_recovery expr;
   init_expr_recovery( &expr, note->expr_start, note->expr_end );
   recover_expr( recovery, &expr );
   struct expr_stmt* stmt = mem_alloc( sizeof( *stmt ) );
   stmt->node.type = NODE_EXPRSTMT;
   stmt->expr = expr.output_node;
   stmt_recovery->output_node = &stmt->node;
   seek_pcode( &stmt_recovery->range, note->exit );
}


static void init_range( struct pcode_range* range, struct pcode* start,
   struct pcode* end ) {
   range->start = start;
   range->end = end;
   seek_pcode( range, range->start );
}

static bool have_pcode( struct pcode_range* range ) {
   return ( range->pcode->obj_pos <= range->end->obj_pos );
}

static void seek_pcode( struct pcode_range* range, struct pcode* pcode ) {
   range->pcode = pcode;
   range->jump = NULL;
   range->casejump = NULL;
   range->sortedcasejump = NULL;
   range->generic = NULL;
   switch ( pcode->opcode ) {
   case PCD_GOTO:
   case PCD_IFGOTO:
   case PCD_IFNOTGOTO:
      range->jump = ( struct jump_pcode* ) pcode;
      break;
   case PCD_CASEGOTO:
      range->casejump = ( struct casejump_pcode* ) pcode;
      break;
   case PCD_CASEGOTOSORTED:
      range->sortedcasejump = ( struct sortedcasejump_pcode* ) pcode;
      break;
   default:
      range->generic = ( struct generic_pcode* ) pcode;
   }
}

static void next_pcode( struct pcode_range* range ) {
   seek_pcode( range, range->pcode->next );
}
