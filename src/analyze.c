/*

   Stage 4 -- Polish

   In the "Polish" stage, we attempt to make the generated ACS code (done
   after this stage) more readable. This includes:
     - Replace variable assignment with variable declaration
     - Replace integral arguments for standard functions with named constants
     - Break down flag arguments into an expression of ORed named constants

*/

#include <stdio.h>

#include "task.h"

struct analysis {
   struct task* task;
};

struct stmt_analysis {
   struct node* replacement;
};

struct predefined_constant {
   const char* name;
   i32 value;
};

struct expr_analysis {
   struct node* replacement;
};

struct result {
   struct var* var;
   struct func* func;
   struct node* replacement;
   const struct predefined_constant* constants;
};

static void init_analysis( struct analysis* analysis, struct task* task );
static void analyze_scripts( struct analysis* analysis );
static void analyze_script( struct analysis* analysis,
   struct script* script );
static void analyze_block( struct analysis* analysis, struct block* block );
static void analyze_stmt( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct node* node );
static void analyze_if( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct if_stmt* stmt );
static void analyze_expr_stmt( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct expr_stmt* stmt );
static void init_expr_analysis( struct expr_analysis* analysis );
static void analyze_expr( struct analysis* analysis,
   struct expr_analysis* expr_analysis, struct expr* expr );
static void init_result( struct result* result );
static void analyze_operand( struct analysis* analysis, struct result* result,
   struct node* node );
static void analyze_binary( struct analysis* analysis, struct result* result,
   struct binary* binary );
static void analyze_assign( struct analysis* analysis, struct result* result,
   struct assign* assign );
static void analyze_prefix( struct analysis* analysis, struct result* result,
   struct node* node );
static void analyze_suffix( struct analysis* analysis, struct result* result,
   struct node* node );
static void analyze_call( struct analysis* analysis, struct result* result,
   struct call* call );
static void decompose_args( struct call* call, ... );
static void analyze_primary( struct analysis* analysis, struct result* result,
   struct node* node );
static void analyze_var( struct analysis* analysis, struct result* result,
   struct var* var );
static void analyze_func( struct analysis* analysis, struct result* result,
   struct func* func );

void t_analyze( struct task* task ) {
   struct analysis analysis;
   init_analysis( &analysis, task );
   analyze_scripts( &analysis );
}

static void init_analysis( struct analysis* analysis, struct task* task ) {
   analysis->task = task;
}

static void analyze_scripts( struct analysis* analysis ) {
   struct list_iter i;
   list_iterate( &analysis->task->scripts, &i );
   while ( ! list_end( &i ) ) {
      analyze_script( analysis, list_data( &i ) );
      list_next( &i );
   }
}

static void analyze_script( struct analysis* analysis,
   struct script* script ) {
   analyze_block( analysis, script->body );
}

static void analyze_block( struct analysis* analysis, struct block* block ) {
   struct list_iter i;
   list_iterate( &block->stmts, &i );
   while ( ! list_end( &i ) ) {
      struct stmt_analysis stmt_analysis = { NULL };
      analyze_stmt( analysis, &stmt_analysis, list_data( &i ) );
      if ( stmt_analysis.replacement ) {
         struct node* replaced_node = list_replace( &block->stmts, &i,
            stmt_analysis.replacement );
         // t_free_node( replaced_node );
      }
      list_next( &i );
   }
}

static void analyze_stmt( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct node* node ) {
   switch ( node->type ) {
   case NODE_IF:
      analyze_if( analysis, stmt_analysis,
         ( struct if_stmt* ) node );
      break;
   case NODE_EXPRSTMT:
      analyze_expr_stmt( analysis, stmt_analysis,
         ( struct expr_stmt* ) node );
      break;
   default:
      break;
   }
}

static void analyze_if( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct if_stmt* stmt ) {
   struct expr_analysis expr_analysis;
   init_expr_analysis( &expr_analysis );
   analyze_expr( analysis, &expr_analysis, stmt->cond );
   analyze_block( analysis, stmt->body );
}

static void analyze_expr_stmt( struct analysis* analysis,
   struct stmt_analysis* stmt_analysis, struct expr_stmt* stmt ) {
   struct expr_analysis expr_analysis;
   init_expr_analysis( &expr_analysis );
   analyze_expr( analysis, &expr_analysis, stmt->expr );
   stmt_analysis->replacement = expr_analysis.replacement;
   //if ( expr_analysis->declare_var ) {
   //   stmt_analysis->replacement = expr_analysis->var;
   //}
}

static void init_expr_analysis( struct expr_analysis* analysis ) {
   analysis->replacement = NULL;
}

static void analyze_expr( struct analysis* analysis,
   struct expr_analysis* expr_analysis, struct expr* expr ) {
   struct result result;
   init_result( &result );
   analyze_operand( analysis, &result, expr->root );
   expr_analysis->replacement = result.replacement;
}

static void init_result( struct result* result ) {
   result->var = NULL;
   result->func = NULL;
   result->replacement = NULL;
   result->constants = NULL;
}
   
static void analyze_operand( struct analysis* analysis, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_BINARY:
      analyze_binary( analysis, result,
         ( struct binary* ) node );
      break;
   case NODE_ASSIGN:
      analyze_assign( analysis, result,
         ( struct assign* ) node );
      break;
   default:
      analyze_prefix( analysis, result,
         node );
   }
}

static void analyze_binary( struct analysis* analysis, struct result* result,
   struct binary* binary ) {
   struct result lside;
   init_result( &lside );
   analyze_operand( analysis, &lside, binary->lside );
   struct result rside;
   init_result( &rside );
   analyze_operand( analysis, &rside, binary->rside );
   if ( binary->rside->type == NODE_LITERAL && lside.constants ) {
      struct literal* literal = ( struct literal* ) binary->rside;
      for ( u32 i = 0; lside.constants[ i ].name[ 0 ] != '\0'; ++i ) {
         if ( lside.constants[ i ].value == literal->value ) {
            struct name_usage* usage = mem_alloc( sizeof( *usage ) );
            usage->node.type = NODE_NAMEUSAGE;
            usage->name = lside.constants[ i ].name;
            binary->rside = &usage->node;
         }
      }
   }
}

static void analyze_assign( struct analysis* analysis, struct result* result,
   struct assign* assign ) {
   struct result lside;
   init_result( &lside );
   analyze_operand( analysis, &lside, assign->lside );
   if ( lside.var && ! lside.var->declared ) {
/*
      struct dec* dec = mem_alloc( sizeof( *dec ) );
      dec->node.type = NODE_DEC;
      dec->var = var;
*/
      struct expr* expr = mem_alloc( sizeof( *expr ) );
      expr->node.type = NODE_EXPR;
      expr->root = assign->rside;
      lside.var->initz = expr;
      lside.var->declared = true;
      result->replacement = &lside.var->node;
      // expr_analysis
   }
   //if ( new_var ) {
   //   expr_analysis->declare_var = true;
   //}
}

static void analyze_prefix( struct analysis* analysis, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_UNARY:
     // emit_unary( codegen,
      //   ( struct unary* ) node );
      break;
   case NODE_INC:
     // visit_inc( codegen,
      //   ( struct inc* ) node );
      break;
   default:
      analyze_suffix( analysis, result, node );
   }
}

static void analyze_suffix( struct analysis* analysis, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_INCPOST:
      //visit_post_inc( codegen,
      //   ( struct inc* ) node );
      break;
   case NODE_SUBSCRIPT:
      //visit_subscript( codegen,
       //  ( struct subscript* ) node );
      break;
   case NODE_CALL:
      analyze_call( analysis, result,
         ( struct call* ) node );
      break;
   default:
      analyze_primary( analysis, result,
         node );
   }
}

static void analyze_primary( struct analysis* analysis, struct result* result,
   struct node* node ) {
   switch ( node->type ) {
   case NODE_VAR:
      analyze_var( analysis, result,
         ( struct var* ) node );
      break;
   case NODE_FUNC:
      analyze_func( analysis, result,
         ( struct func* ) node );
      break;
   default:
      break;
   }
}

static void analyze_var( struct analysis* analysis, struct result* result,
   struct var* var ) {
   result->var = var;
}

static void analyze_func( struct analysis* analysis, struct result* result,
   struct func* func ) {
   result->func = func;
}

struct predef_group {
   struct predef_entry {
      const char* name;
      i32 value;
   }* entries;
   u32 size;
   bool flags;
};

/*
static const struct predef_group g_line = {
   ( struct predef_entry[] ) {
      { "LINE_FRONT", 0 },
      { "LINE_BACK", 1 },
   },
   ARRAY_SIZE( 
};
*/

static const struct predefined_constant
g_predefs[] = {
   { "T_NONE", 0 },
   { "T_SHOTGUY", 0 },
};

static const struct predefined_constant g_line[] = {
   { "LINE_FRONT", 0 },
   { "LINE_BACK", 1 },
   { "", 0 },
};

static const struct predefined_constant g_side[] = {
   { "SIDE_FRONT", 0 },
   { "SIDE_BACK", 1 },
   { "", 0 },
};

#define GROUP( name, ... ) \
   static const struct predefined_constant g_##name[] = { __VA_ARGS__ }

GROUP( texture,
   { "TEXTURE_TOP", 0 },
   { "TEXTURE_MIDDLE", 1 },
   { "TEXTURE_BOTTOM", 2 },
   { "", 0 },
);

static const struct predefined_constant
g_texflag[] = {
   { "TEXFLAG_TOP", 0x1 },
   { "TEXFLAG_MIDDLE", 0x2 },
   { "TEXFLAG_BOTTOM", 0x4 },
   { "TEXFLAG_ADDOFFSET", 0x8 },
};

static const struct predefined_constant
g_game[] = {
   { "GAME_SINGLE_PLAYER", 0 },
   { "GAME_NET_COOPERATIVE", 1 },
   { "GAME_NET_DEATHMATCH", 2 },
   { "GAME_TITLE_MAP", 3 },
};

static const struct predefined_constant g_skip[ 1 ] = { { "", 0 } };
static const struct predefined_constant g_done[ 1 ] = { { "", 0 } };

static const struct predefined_constant** g_funcs[] = {
   #define ENTRY( ... ) \
      ( const struct predefined_constant*[] ) { __VA_ARGS__ }
   ENTRY( NULL, NULL, NULL, g_side, g_texflag ),
   #undef ENTRY
};

static void analyze_call( struct analysis* analysis, struct result* result,
   struct call* call ) {
   struct result operand;
   init_result( &operand );
   analyze_operand( analysis, &operand, call->operand );
   if ( operand.func->type == FUNC_DED ) {
      switch ( operand.func->more.ded->opcode ) {
      case PCD_SETLINETEXTURE:
         decompose_args( call,
            g_skip,
            g_side,
            g_texture,
            g_skip,
            g_done );
         break;
      case PCD_GAMETYPE:
         result->constants = g_game;
         break;
      }
   }
/*
   if ( call->func->type == FUNC_DED ) {
      switch ( call->func->more.ded.opcode ) {
      case PCD_THINGCOUNT:
         {
            list_iter_t i;
            list_iter_init( &i, &call->args );
            while ( ! list_end( &i ) ) {
               if ( arg_number == 0 ) {
                  if ( arg->folded ) {
                     switch ( arg->value ) {
                     case 0:
                        "
                     }
                  }
               }
               list_next( &i );
            }
         }
         break;
      }
   }
printf( "a\n" );*/
}

static void decompose_args( struct call* call, ... ) {
   va_list args;
   va_start( args, call );
   struct list_iter i;
   list_iterate( &call->args, &i );
   while ( ! list_end( &i ) ) {
      struct expr* expr = list_data( &i );
      struct predefined_constant* group = va_arg( args,
         struct predefined_constant* );
      if ( group == g_skip ) {
      list_next( &i );
         continue;
      }
      if ( group == g_done ) {
         break;
      }
      if ( expr->root->type == NODE_LITERAL ) {
         struct literal* literal = ( struct literal* ) expr->root;
         for ( u32 i = 0; group[ i ].name[ 0 ] != '\0'; ++i ) {
            if ( group[ i ].value == literal->value ) {
               struct name_usage* usage = mem_alloc( sizeof( *usage ) );
               usage->node.type = NODE_NAMEUSAGE;
               usage->name = group[ i ].name;
               expr->root = &usage->node;
            }
         }
      }
      list_next( &i );
   }
   va_end( args );
}
