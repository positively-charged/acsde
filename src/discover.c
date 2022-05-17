/*

   Stage 2 -- Annotation

   In the "Annotation" stage, we look for common instruction sequences in the
   loaded p-code and keep notes of that.

*/

#include <stdio.h>

#include "task.h"
#include "pcode.h"

struct discovery {
   struct task* task;
};

struct stmt_discovery {
   struct stmt_discovery* parent;
   struct pcode_range range;
   i32 break_obj_pos;
   i32 continue_obj_pos;
   bool break_stmt;
   bool continue_stmt;
};

struct for_discovery {
   struct jump_pcode* cond_jump;
   struct jump_pcode* post_jump;
   struct jump_pcode* exit_jump;
   struct list post;
   bool discovered;
};

struct expr_discovery {
   jmp_buf bail;
   struct pcode_range range;
   struct pcode* start;
   struct pcode* end;
   struct pcode* exit;
   i32 stack_size;
   i32 print_depth;
   i32 more_args;
   i32 optional_args;
   bool discovered;
   bool done;
   bool more_args_given;
   bool optional_args_given;
   bool translation;
};

static void init_discovery( struct discovery* discovery, struct task* task );
static void examine_script_list( struct discovery* discovery );
static void examine_script( struct discovery* discovery,
   struct script* script );
static void examine_func_list( struct discovery* discovery );
static void examine_func( struct discovery* discovery, struct func* func );
static void init_stmt_discovery( struct stmt_discovery* stmt,
   struct stmt_discovery* parent, struct pcode* start, struct pcode* end );
static void init_range( struct pcode_range* range, struct pcode* start,
   struct pcode* end );
static bool have_pcode( struct pcode_range* range );
static void seek_pcode( struct pcode_range* range, struct pcode* pcode );
static void next_pcode( struct pcode_range* range );
static bool in_range( struct pcode_range* range, struct pcode* pcode );
static void examine_block( struct discovery* discovery,
   struct stmt_discovery* block );
static void examine_stmt( struct discovery* discovery,
   struct stmt_discovery* stmt );
static void examine_goto( struct discovery* discovery,
   struct stmt_discovery* stmt_discovery );
static void examine_break( struct stmt_discovery* stmt );
static void examine_continue( struct stmt_discovery* stmt );
static void examine_goto_down( struct discovery* discovery,
   struct stmt_discovery* stmt );
static struct loop_note* alloc_loop_note( void );
static void examine_expr( struct discovery* discovery,
   struct stmt_discovery* stmt );
static void init_expr_discovery( struct expr_discovery* discovery,
   struct pcode* start, struct pcode* end );
static void push( struct expr_discovery* discovery, i32 amount );
static void pop( struct expr_discovery* discovery, i32 amount );
static void bail( struct expr_discovery* discovery );
static void discover_expr( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_operand( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_ded( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_ded_direct( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_call_user( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_call_aspec( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_call_ext( struct discovery* discovery,
   struct expr_discovery* expr_discovery );
static void examine_expr_ifgoto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void init_for_discovery( struct for_discovery* discovery );
static void discover_for( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr,
   struct for_discovery* for_discovery );
static void examine_for( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr,
   struct for_discovery* for_discovery );
static void examine_expr_ifnotgoto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_expr_ifnotgoto_upperaddr( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_while( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_if( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_expr_ifnotgoto_loweraddr( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_do( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static struct do_note* alloc_do( void );
static struct if_note* alloc_if_note( void );
static void init_note( struct note* note, i32 type );
static void append_note( struct pcode* pcode, struct note* note );
static void examine_expr_goto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_switch( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_returnval( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr );
static void examine_expr_stmt( struct stmt_discovery* stmt,
   struct expr_discovery* expr );

void t_annotate( struct task* task ) {
   struct discovery discovery;
   init_discovery( &discovery, task );
   examine_script_list( &discovery );
   examine_func_list( &discovery );
}

static void init_discovery( struct discovery* discovery, struct task* task ) {
   discovery->task = task;
}

static void examine_script_list( struct discovery* discovery ) {
   struct list_iter i;
   list_iterate( &discovery->task->scripts, &i );
   while ( ! list_end( &i ) ) {
      examine_script( discovery, list_data( &i ) );
      list_next( &i );
   }
}

static void examine_script( struct discovery* discovery,
   struct script* script ) {
   struct stmt_discovery body;
   init_stmt_discovery( &body, NULL,
      script->body_start,
      script->body_end );
   examine_block( discovery, &body );

/*
   while ( have_pcode( &body.range ) ) {
      printf( "> %d", body.range.pcode->opcode );
      if ( body.range.generic ) {
         struct generic_pcode_arg* arg = body.range.generic->args;
         while ( arg ) {
            printf( " %d", arg->value );
            arg = arg->next;
         }
      }
      next_pcode( &body.range );
      printf( "\n" );
   }
t_bail( discovery->task );
*/
}

static void examine_func_list( struct discovery* discovery ) {
   struct list_iter i;
   list_iterate( &discovery->task->funcs, &i );
   while ( ! list_end( &i ) ) {
      examine_func( discovery, list_data( &i ) );
      list_next( &i );
   }
}

static void examine_func( struct discovery* discovery, struct func* func ) {
   struct stmt_discovery body;
   init_stmt_discovery( &body, NULL,
      func->more.user->start,
      func->more.user->end );
   examine_block( discovery, &body );
}

static void init_stmt_discovery( struct stmt_discovery* stmt,
   struct stmt_discovery* parent, struct pcode* start, struct pcode* end ) {
   stmt->parent = parent;
   init_range( &stmt->range, start, end );
   stmt->break_obj_pos = 0;
   stmt->continue_obj_pos = 0;
   stmt->break_stmt = false;
   stmt->continue_stmt = false;
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

static bool in_range( struct pcode_range* range, struct pcode* pcode ) {
   return ( pcode->obj_pos >= range->start->obj_pos &&
      pcode->obj_pos <= range->end->obj_pos );
}

static void examine_block( struct discovery* discovery,
   struct stmt_discovery* block ) {
   while ( have_pcode( &block->range ) ) {
      struct stmt_discovery stmt;
      init_stmt_discovery( &stmt, block, block->range.pcode,
         block->range.end );
      examine_stmt( discovery, &stmt );
      seek_pcode( &block->range, stmt.range.pcode );
   }
}

static void examine_stmt( struct discovery* discovery,
   struct stmt_discovery* stmt ) {
   switch ( stmt->range.pcode->opcode ) {
   case PCD_GOTO:
      examine_goto( discovery, stmt );
      break;
   default:
      examine_expr( discovery, stmt );
   }
}

static void examine_goto( struct discovery* discovery,
   struct stmt_discovery* stmt_discovery ) {
   // Check for break statement.
   struct stmt_discovery* target = stmt_discovery->parent;
   while ( target && ! target->break_stmt ) {
      target = target->parent;
   }
   if ( target && stmt_discovery->range.jump->destination->obj_pos ==
      target->break_obj_pos ) {
      examine_break( stmt_discovery );
      return;
   }
   // Check for continue statement.
   target = stmt_discovery->parent;
   while ( target && ! target->continue_stmt ) {
      target = target->parent;
   }
   if ( target && stmt_discovery->range.jump->destination->obj_pos ==
      target->continue_obj_pos ) {
      examine_continue( stmt_discovery );
      return;
   }
   if ( stmt_discovery->range.jump->destination->obj_pos >
      stmt_discovery->range.pcode->obj_pos ) {
      examine_goto_down( discovery, stmt_discovery );
   }
   else {
      next_pcode( &stmt_discovery->range );
   }
}

static void examine_break( struct stmt_discovery* stmt ) {
   struct jump_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_JUMP );
   note->stmt = JUMPNOTE_BREAK;
   append_note( stmt->range.pcode, &note->note );
   next_pcode( &stmt->range );
}

static void examine_continue( struct stmt_discovery* stmt ) {
   struct jump_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_JUMP );
   note->stmt = JUMPNOTE_CONTINUE;
   append_note( stmt->range.pcode, &note->note );
   next_pcode( &stmt->range );
}

static void examine_goto_down( struct discovery* discovery,
   struct stmt_discovery* stmt ) {
   struct expr_discovery expr;
   init_expr_discovery( &expr, stmt->range.jump->destination,
      stmt->range.end );
   discover_expr( discovery, &expr );
   if ( expr.discovered && ( expr.exit->next->opcode == PCD_IFGOTO ||
      expr.exit->next->opcode == PCD_IFNOTGOTO ) ) {
      struct jump_pcode* body_jump = ( struct jump_pcode* ) expr.exit->next;
      if ( body_jump->destination == stmt->range.jump->pcode.next ) {
         struct loop_note* note = alloc_loop_note();
         note->cond_start = expr.start;
         note->body_start = stmt->range.pcode->next;
         note->exit = body_jump->pcode.next;
         if ( stmt->range.pcode->opcode == PCD_IFNOTGOTO ) {
            note->until = true;
         }
         // append_note_before( discovery->pcode, &note->note );
         // discovery->pcode = body_jump->pcode.exit;
      }
      else {
      next_pcode( &stmt->range );
      }
   }
   else {
      next_pcode( &stmt->range );
   }
}

static struct loop_note* alloc_loop_note( void ) {
   struct loop_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_LOOP );
   note->cond_start = NULL;
   note->cond_end = NULL;
   note->body_start = NULL;
   note->body_end = NULL;
   note->exit = NULL;
   note->until = false;
   return note;
}

static void examine_expr( struct discovery* discovery,
   struct stmt_discovery* stmt ) {
   struct expr_discovery expr;
   init_expr_discovery( &expr, stmt->range.pcode, stmt->range.end );
   discover_expr( discovery, &expr );
   if ( expr.discovered ) {
      if ( in_range( &stmt->range, expr.exit ) ) {
         seek_pcode( &stmt->range, expr.exit );
         switch ( stmt->range.pcode->opcode ) {
         case PCD_IFGOTO:
            examine_expr_ifgoto( discovery, stmt, &expr );
            break;
         case PCD_IFNOTGOTO:
            examine_expr_ifnotgoto( discovery, stmt, &expr );
            break;
         case PCD_GOTO:
            examine_expr_goto( discovery, stmt, &expr );
            break;
         case PCD_RETURNVAL:
            examine_returnval( discovery, stmt, &expr );
            break;
         default:
            examine_expr_stmt( stmt, &expr );
         // case PCD_CASEGOTO:
            // examine_casegoto( discovery );
         //    break;
         }
      }
      else {
         examine_expr_stmt( stmt, &expr );
      }
   }
   else {
      next_pcode( &stmt->range );
   }
}

static void init_expr_discovery( struct expr_discovery* discovery,
   struct pcode* start, struct pcode* end ) {
   init_range( &discovery->range, start, end );
   discovery->start = start;
   discovery->end = NULL;
   discovery->exit = NULL;
   discovery->stack_size = 0;
   discovery->print_depth = 0;
   discovery->discovered = false;
   discovery->more_args_given = false;
   discovery->optional_args_given = false;
   discovery->done = false;
   discovery->translation = false;
}

static void push( struct expr_discovery* discovery, i32 amount ) {
   discovery->stack_size += amount;
}

static void pop( struct expr_discovery* discovery, i32 amount ) {
   if ( discovery->stack_size >= amount ) {
      discovery->stack_size -= amount;
   }
   else {
      bail( discovery );
   }
}

static void bail( struct expr_discovery* discovery ) {
   longjmp( discovery->bail, 1 );
}

static void discover_expr( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   if ( setjmp( expr_discovery->bail ) == 0 ) {
      while ( ! expr_discovery->done ) {
         examine_operand( discovery, expr_discovery );
      }
      expr_discovery->exit = expr_discovery->range.pcode;
      expr_discovery->end = expr_discovery->exit->prev;
      expr_discovery->discovered = true;
   }
}

static void examine_operand( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   switch ( expr_discovery->range.pcode->opcode ) {
   case PCD_PUSHNUMBER:
   case PCD_PUSHBYTE:
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PUSH2BYTES:
   case PCD_PUSH3BYTES:
   case PCD_PUSH4BYTES:
   case PCD_PUSH5BYTES:
      push( expr_discovery, expr_discovery->range.pcode->opcode -
         PCD_PUSH2BYTES + 2 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PUSHBYTES:
      push( expr_discovery, expr_discovery->range.generic->args->value );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_DUP:
      if ( ! ( expr_discovery->stack_size > 0 ) ) {
         bail( expr_discovery );
      }
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_DROP:
      if ( expr_discovery->stack_size > 1 &&
         expr_discovery->print_depth == 0 &&
         ! expr_discovery->translation ) {
         pop( expr_discovery, 1 );
      }
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PUSHSCRIPTVAR:
   case PCD_PUSHMAPVAR:
   case PCD_PUSHWORLDVAR:
   case PCD_PUSHGLOBALVAR:
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      switch ( expr_discovery->range.pcode->opcode ) {
      case PCD_INCSCRIPTVAR:
      case PCD_INCMAPVAR:
      case PCD_INCWORLDVAR:
         next_pcode( &expr_discovery->range );
         break;
      default:
         break;
      }
      break;
   case PCD_PUSHSCRIPTARRAY:
   case PCD_PUSHMAPARRAY:
   case PCD_PUSHWORLDARRAY:
   case PCD_PUSHGLOBALARRAY:
      pop( expr_discovery, 1 );
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_UNARYMINUS:
   case PCD_NEGATELOGICAL:
   case PCD_NEGATEBINARY:
   case PCD_TAGSTRING:
      pop( expr_discovery, 1 );
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_INCSCRIPTVAR:
   case PCD_INCMAPVAR:
   case PCD_INCWORLDVAR:
   case PCD_INCGLOBALVAR:
   case PCD_DECSCRIPTVAR:
   case PCD_DECMAPVAR:
   case PCD_DECWORLDVAR:
   case PCD_DECGLOBALVAR:
      next_pcode( &expr_discovery->range );
      switch ( expr_discovery->range.pcode->opcode ) {
      case PCD_PUSHSCRIPTVAR:
      case PCD_PUSHMAPVAR:
      case PCD_PUSHWORLDVAR:
         push( expr_discovery, 1 );
         next_pcode( &expr_discovery->range );
         break;
      default:
         break;
      }
      break;
   case PCD_INCSCRIPTARRAY:
   case PCD_INCMAPARRAY:
   case PCD_INCWORLDARRAY:
   case PCD_INCGLOBALARRAY:
   case PCD_DECSCRIPTARRAY:
   case PCD_DECMAPARRAY:
   case PCD_DECWORLDARRAY:
   case PCD_DECGLOBALARRAY:
      pop( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      switch ( expr_discovery->range.pcode->opcode ) {
      case PCD_PUSHSCRIPTARRAY:
      case PCD_PUSHMAPARRAY:
      case PCD_PUSHWORLDARRAY:
         pop( expr_discovery, 1 );
         push( expr_discovery, 1 );
         next_pcode( &expr_discovery->range );
         break;
      default:
         break;
      }
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
      pop( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      if ( expr_discovery->stack_size == 0 ) {
         expr_discovery->done = true;
      }
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
      pop( expr_discovery, 2 );
      next_pcode( &expr_discovery->range );
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
      examine_ded( discovery, expr_discovery );
      if ( expr_discovery->stack_size == 0 ) {
         expr_discovery->done = true;
      }
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
      examine_ded_direct( discovery, expr_discovery );
      if ( expr_discovery->stack_size == 0 ) {
         expr_discovery->done = true;
      }
      break;
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
      pop( expr_discovery, 2 );
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_BEGINPRINT:
      ++expr_discovery->print_depth;
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PRINTSTRING:
   case PCD_PRINTNUMBER:
   case PCD_PRINTCHARACTER:
   case PCD_PRINTNAME:
   case PCD_PRINTFIXED:
   case PCD_PRINTLOCALIZED:
   case PCD_PRINTBIND:
   case PCD_PRINTBINARY:
   case PCD_PRINTHEX:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PRINTMAPCHARARRAY:
   case PCD_PRINTWORLDCHARARRAY:
   case PCD_PRINTGLOBALCHARARRAY:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 2 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_PRINTMAPCHRANGE:
   case PCD_PRINTWORLDCHRANGE:
   case PCD_PRINTGLOBALCHRANGE:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 4 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_ENDPRINT:
   case PCD_ENDPRINTBOLD:
   case PCD_ENDLOG:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      --expr_discovery->print_depth;
      next_pcode( &expr_discovery->range );
      break;
   case PCD_SAVESTRING:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      --expr_discovery->print_depth;
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_MOREHUDMESSAGE:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      expr_discovery->more_args = expr_discovery->stack_size;
      expr_discovery->more_args_given = true;
      next_pcode( &expr_discovery->range );
      break;
   case PCD_OPTHUDMESSAGE:
      if ( ! ( expr_discovery->print_depth > 0 &&
         expr_discovery->more_args_given ) ) {
         bail( expr_discovery );
      }
      expr_discovery->optional_args = expr_discovery->stack_size;
      expr_discovery->optional_args_given = true;
      next_pcode( &expr_discovery->range );
      break;
   case PCD_ENDHUDMESSAGE:
   case PCD_ENDHUDMESSAGEBOLD:
      if ( ! ( expr_discovery->print_depth > 0 ) ) {
         bail( expr_discovery );
      }
      if ( ! expr_discovery->more_args_given ) {
         bail( expr_discovery );
      }
      if ( expr_discovery->optional_args_given ) {
         pop( expr_discovery, expr_discovery->stack_size -
            expr_discovery->optional_args );
      }
      pop( expr_discovery, expr_discovery->stack_size -
         expr_discovery->more_args );
      next_pcode( &expr_discovery->range );
      --expr_discovery->print_depth;
      break;
   case PCD_STRCPYTOMAPCHRANGE:
   case PCD_STRCPYTOWORLDCHRANGE:
   case PCD_STRCPYTOGLOBALCHRANGE:
      pop( expr_discovery, 6 );
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_LSPEC1:
   case PCD_LSPEC2:
   case PCD_LSPEC3:
   case PCD_LSPEC4:
   case PCD_LSPEC5:
      examine_call_aspec( discovery, expr_discovery );
      break;
   case PCD_LSPEC5EX:
      pop( expr_discovery, 5 );
      next_pcode( &expr_discovery->range );
      break;
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
      next_pcode( &expr_discovery->range );
      if ( expr_discovery->stack_size == 0 ) {
         expr_discovery->done = true;
      }
      else {
         bail( expr_discovery );
      }
      break;
   case PCD_LSPEC5RESULT:
   case PCD_LSPEC5EXRESULT:
      pop( expr_discovery, 5 );
      push( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_CALL:
   case PCD_CALLDISCARD:
      examine_call_user( discovery, expr_discovery );
      break;
   case PCD_CALLFUNC:
      examine_call_ext( discovery, expr_discovery );
      break;
   case PCD_STARTTRANSLATION:
      if ( expr_discovery->translation ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 1 );
      next_pcode( &expr_discovery->range );
      expr_discovery->translation = true;
      break;
   case PCD_TRANSLATIONRANGE1:
      if ( ! expr_discovery->translation ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 4 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_TRANSLATIONRANGE2:
   case PCD_TRANSLATIONRANGE3:
      if ( ! expr_discovery->translation ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 8 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_TRANSLATIONRANGE4:
      if ( ! expr_discovery->translation ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 5 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_TRANSLATIONRANGE5:
      if ( ! expr_discovery->translation ) {
         bail( expr_discovery );
      }
      pop( expr_discovery, 6 );
      next_pcode( &expr_discovery->range );
      break;
   case PCD_ENDTRANSLATION:
      if ( ! expr_discovery->translation ) {
         bail( expr_discovery );
      }
      next_pcode( &expr_discovery->range );
      expr_discovery->done = true;
      break;
   default:
      if ( expr_discovery->stack_size == 1 &&
         expr_discovery->print_depth == 0 &&
         ! expr_discovery->translation ) {
         expr_discovery->done = true;
      }
      else {
         bail( expr_discovery );
      }
   }
   if ( expr_discovery->stack_size == 0 &&
      expr_discovery->print_depth == 0 &&
      ! expr_discovery->translation ) {
      expr_discovery->done = true;
   }
}

static void examine_ded( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   struct func* func = t_get_ded_func( discovery->task,
      expr_discovery->range.pcode->opcode );
   pop( expr_discovery, func->max_param );
   if ( func->return_spec != SPEC_VOID ) {
      push( expr_discovery, 1 );
   }
   next_pcode( &expr_discovery->range );
}

static void examine_ded_direct( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   struct func* func = t_get_ded_func( discovery->task,
      expr_discovery->range.pcode->opcode );
   if ( func->return_spec != SPEC_VOID ) {
      push( expr_discovery, 1 );
   }
   next_pcode( &expr_discovery->range );
}

static void examine_call_aspec( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   i32 id = expr_discovery->range.generic->args->value;
   pop( expr_discovery, expr_discovery->range.pcode->opcode - PCD_LSPEC1 + 1 );
   next_pcode( &expr_discovery->range );
   enum { ASPEC_ACSEXECUTE = 80 };
   if ( id == ASPEC_ACSEXECUTE ) {
      if ( expr_discovery->range.pcode->opcode == PCD_SCRIPTWAIT ) {
         struct intern_func_note* note = mem_alloc( sizeof( *note ) );
         init_note( &note->note, NOTE_INTERNFUNC );
         note->func = t_find_intern_func( discovery->task,
            INTERNFUNC_ACSEXECUTEWAIT );
         append_note( expr_discovery->range.pcode->prev, &note->note );
         pop( expr_discovery, 1 );
         next_pcode( &expr_discovery->range );
         note->exit = expr_discovery->range.pcode;
      }
   }
   if ( expr_discovery->stack_size == 0 ) {
      expr_discovery->done = true;
   }
   else {
      bail( expr_discovery );
   }
}

static void examine_call_user( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   struct func* func = t_find_func( discovery->task,
      ( u32 ) expr_discovery->range.generic->args->value );
   if ( ! func ) {
      bail( expr_discovery );
   }
   pop( expr_discovery, func->max_param );
   if ( expr_discovery->range.pcode->opcode == PCD_CALL ) {
      if ( func->return_spec == SPEC_VOID ) {
         t_diag( discovery->task, DIAG_ERR,
            "encounted a `call` instruction whose argument is a function that "
            "has a void return type" );
         t_bail( discovery->task );
      }
      push( expr_discovery, 1 );
   }
   next_pcode( &expr_discovery->range );
}

static void examine_call_ext( struct discovery* discovery,
   struct expr_discovery* expr_discovery ) {
   i32 id = expr_discovery->range.generic->args->next->value;
   pop( expr_discovery, expr_discovery->range.generic->args->value );
   push( expr_discovery, 1 );
   next_pcode( &expr_discovery->range );
   enum { EXTFUNC_ACSNAMEDEXECUTE = 39 };
   if ( id == EXTFUNC_ACSNAMEDEXECUTE ) {
      if ( expr_discovery->range.pcode->opcode == PCD_DROP &&
         expr_discovery->range.pcode->next->opcode == PCD_SCRIPTWAITNAMED ) {
         struct intern_func_note* note = mem_alloc( sizeof( *note ) );
         init_note( &note->note, NOTE_INTERNFUNC );
         note->func = t_find_intern_func( discovery->task,
            INTERNFUNC_ACSNAMEDEXECUTEWAIT );
         append_note( expr_discovery->range.pcode->prev, &note->note );
         pop( expr_discovery, 2 );
         next_pcode( &expr_discovery->range );
         next_pcode( &expr_discovery->range );
         note->exit = expr_discovery->range.pcode;
      }
   }
}

static void examine_expr_ifgoto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   if ( in_range( &stmt->range, stmt->range.pcode ) ) {
      struct for_discovery for_discovery;
      init_for_discovery( &for_discovery );
      discover_for( discovery, stmt, expr, &for_discovery );
      if ( for_discovery.discovered ) {
         examine_for( discovery, stmt, expr, &for_discovery );
         return;
      }
      if ( stmt->range.jump->destination->prev->opcode == PCD_GOTO ) {
         struct jump_pcode* exit_jump = ( struct jump_pcode* )
            stmt->range.jump->destination->prev;
         if ( exit_jump->destination == expr->start ) {
            examine_while( discovery, stmt, expr );
            return;
         }
      }
      if ( stmt->range.jump->destination->obj_pos <
         stmt->range.pcode->obj_pos ) {
         examine_do( discovery, stmt, expr );
      }
   }
}

static void init_for_discovery( struct for_discovery* discovery ) {
   discovery->cond_jump = NULL;
   discovery->post_jump = NULL;
   discovery->exit_jump = NULL;
   list_init( &discovery->post );
   discovery->discovered = false;
}

static void discover_for( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr,
   struct for_discovery* for_discovery ) {
   if ( stmt->range.pcode->next->opcode == PCD_GOTO ) {
      struct jump_pcode* exit_jump = ( struct jump_pcode* )
         stmt->range.pcode->next;
      for_discovery->exit_jump = exit_jump;
      if ( exit_jump->destination->prev->opcode == PCD_GOTO ) {
         struct jump_pcode* post_jump = ( struct jump_pcode* )
            exit_jump->destination->prev;
         if ( post_jump->destination == exit_jump->pcode.next ) {
            if ( stmt->range.jump->destination->prev->opcode == PCD_GOTO ) {
               struct jump_pcode* cond_jump = ( struct jump_pcode* )
                  stmt->range.jump->destination->prev;
               if ( cond_jump->destination == expr->start ) {
                  struct pcode* start = exit_jump->pcode.next;
                  while ( start != &cond_jump->pcode ) {
                     struct expr_discovery post_expr;
                     init_expr_discovery( &post_expr, start,
                         cond_jump->pcode.prev );
                     discover_expr( discovery, &post_expr );
                     if ( ! post_expr.discovered ) {
                        return;
                     }
                     struct for_note_post* post = mem_alloc( sizeof( *post ) );
                     post->start = post_expr.start;
                     post->end = post_expr.end;
                     list_append( &for_discovery->post, post );
                     start = post_expr.exit;
                  }
                  for_discovery->post_jump = post_jump;
                  for_discovery->cond_jump = cond_jump;
                  for_discovery->discovered = true;
               }
            }
         }
      }
   }
}

static void examine_for( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr,
   struct for_discovery* for_discovery ) {
   struct for_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_FOR );
   note->cond_start = expr->start;
   note->cond_end = expr->end;
   note->post_start = for_discovery->exit_jump->pcode.next;
   note->post_end = for_discovery->cond_jump->pcode.prev;
   note->body_start = for_discovery->cond_jump->pcode.next;
   note->body_end = for_discovery->post_jump->pcode.prev;
   note->exit = for_discovery->exit_jump->destination;
   note->post = for_discovery->post;
   append_note( expr->start, &note->note );
   stmt->break_stmt = true;
   stmt->break_obj_pos = note->exit->obj_pos;
   stmt->continue_stmt = true;
   stmt->continue_obj_pos = note->post_start->obj_pos;
   struct stmt_discovery body;
   init_stmt_discovery( &body, stmt,
      note->body_start,
      note->body_end );
   examine_block( discovery, &body );
   seek_pcode( &stmt->range, note->exit );
}

static void examine_expr_ifnotgoto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   if ( in_range( &stmt->range, stmt->range.pcode ) ) {
      if ( stmt->range.jump->destination->obj_pos >
         stmt->range.pcode->obj_pos ) {
         examine_expr_ifnotgoto_upperaddr( discovery, stmt, expr );
      }
      else {
         examine_expr_ifnotgoto_loweraddr( discovery, stmt, expr );
      }
   }
   else {
      examine_expr_stmt( stmt, expr );
   }
}

static void examine_expr_ifnotgoto_upperaddr( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   bool while_stmt = false;
   if ( stmt->range.jump->destination->prev->opcode == PCD_GOTO ) {
      struct jump_pcode* jump = ( struct jump_pcode* )
         stmt->range.jump->destination->prev;
      if ( jump->destination == expr->start ) {
         while_stmt = true;
      }
   }
   if ( while_stmt ) {
      examine_while( discovery, stmt, expr );
   }
   else {
      examine_if( discovery, stmt, expr );
   }
}

static void examine_while( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   struct jump_pcode* cond_jump = ( struct jump_pcode* )
      stmt->range.jump->destination->prev;
   struct loop_note* note = alloc_loop_note();
   note->cond_start = expr->start;
   note->cond_end = expr->end;
   note->body_start = stmt->range.pcode->next;
   note->body_end = cond_jump->pcode.prev;
   note->exit = cond_jump->pcode.next;
   note->until = ( stmt->range.pcode->opcode == PCD_IFGOTO );
   append_note( expr->start, &note->note );
   stmt->break_stmt = true;
   stmt->break_obj_pos = note->exit->obj_pos;
   stmt->continue_stmt = true;
   stmt->continue_obj_pos = note->cond_start->obj_pos;
   struct stmt_discovery body;
   init_stmt_discovery( &body, stmt,
      note->body_start,
      note->body_end );
   examine_block( discovery, &body );
   seek_pcode( &stmt->range, note->exit );
}

static void examine_if( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   struct if_note* note = alloc_if_note();
   note->cond_start = expr->start;
   note->cond_end = expr->end;
   note->body_start = stmt->range.pcode->next;
   note->body_end = stmt->range.jump->destination->prev;
   note->exit = stmt->range.jump->destination;
   if ( ( in_range( &stmt->range, stmt->range.jump->destination ) ||
      stmt->range.jump->destination == stmt->range.end->next ) &&
      stmt->range.jump->destination->prev->opcode == PCD_GOTO ) {
      struct jump_pcode* exit_jump = ( struct jump_pcode* )
         stmt->range.jump->destination->prev;
      if ( ( in_range( &stmt->range, exit_jump->destination ) ||
         exit_jump->destination == stmt->range.end->next ) && ( exit_jump->destination->obj_pos > exit_jump->pcode.obj_pos ||
         exit_jump->destination == stmt->range.jump->destination ) ) {
         note->else_body_start = exit_jump->pcode.next;
         note->else_body_end = exit_jump->destination->prev;
         note->body_end = exit_jump->pcode.prev;
         note->exit = exit_jump->destination;
      }
   }
   append_note( expr->start, &note->note );
   struct stmt_discovery body;
   init_stmt_discovery( &body, stmt,
      note->body_start,
      note->body_end );

   examine_block( discovery, &body );
   if ( note->else_body_start ) {
      init_stmt_discovery( &body, stmt,
         note->else_body_start,
         note->else_body_end );
      examine_block( discovery, &body );
   }
   seek_pcode( &stmt->range, note->exit );
}

static struct if_note* alloc_if_note( void ) {
   struct if_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_IF );
   note->cond_start = NULL;
   note->cond_end = NULL;
   note->body_start = NULL;
   note->body_end = NULL;
   note->else_body_start = NULL;
   note->else_body_end = NULL;
   note->exit = NULL;
   return note;
}

static void init_note( struct note* note, i32 type ) {
   note->type = type;
   note->next = NULL;
}

static void append_note( struct pcode* pcode, struct note* note ) {
   note->next = pcode->note;
   pcode->note = note;
}

static void examine_expr_ifnotgoto_loweraddr( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   examine_do( discovery, stmt, expr );
}

static void examine_do( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   struct do_note* note = alloc_do();
   note->cond_start = expr->start;
   note->cond_end = expr->end;
   note->body_start = stmt->range.jump->destination;
   note->body_end = expr->start->prev;
   note->exit = stmt->range.pcode->next;
   note->until = ( stmt->range.pcode->opcode == PCD_IFNOTGOTO );
   stmt->break_stmt = true;
   stmt->break_obj_pos = note->exit->obj_pos;
   stmt->continue_stmt = true;
   stmt->continue_obj_pos = note->cond_start->obj_pos;
   struct stmt_discovery body;
   init_stmt_discovery( &body, stmt,
      note->body_start,
      note->body_end );
   examine_block( discovery, &body );
   append_note( note->body_start, &note->note );
   seek_pcode( &stmt->range, note->exit );
}

static struct do_note* alloc_do( void ) {
   struct do_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_DO );
   note->cond_start = NULL;
   note->cond_end = NULL;
   note->body_start = NULL;
   note->body_end = NULL;
   note->exit = NULL;
   note->until = false;
   return note;
}

static void examine_expr_goto( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   switch ( stmt->range.jump->destination->opcode ) {
   case PCD_CASEGOTO:
   case PCD_CASEGOTOSORTED:
   case PCD_DROP:
      examine_switch( discovery, stmt, expr );
      break;
   default:
      examine_expr_stmt( stmt, expr );
   }
}

static void examine_switch( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   struct switch_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_SWITCH );
   note->cond_start = expr->start;
   note->cond_end = expr->end;
   note->body_start = stmt->range.pcode->next;
   note->body_end = stmt->range.jump->destination->prev->prev;
   note->case_start = NULL;
   note->case_end = NULL;
   note->sorted_jump = NULL;
   note->exit = NULL;
   append_note( expr->start, &note->note );
   if ( stmt->range.jump->destination->opcode == PCD_CASEGOTOSORTED ) {
      seek_pcode( &stmt->range, stmt->range.jump->destination );
      note->sorted_jump = stmt->range.sortedcasejump;
      next_pcode( &stmt->range );
   }
   else {
      struct pcode_range range;
      init_range( &range, stmt->range.jump->destination, stmt->range.end );
      if ( range.pcode->opcode == PCD_CASEGOTO ) {
         note->case_start = range.pcode;
         while ( have_pcode( &range ) && range.pcode->opcode == PCD_CASEGOTO ) {
            note->case_end = range.pcode;
            next_pcode( &range );
         }
         seek_pcode( &stmt->range, range.pcode );
      }
   }
   next_pcode( &stmt->range );
   struct pcode* default_case = NULL;
   if ( stmt->range.pcode->opcode == PCD_GOTO &&
      in_range( &stmt->range, stmt->range.jump->destination ) ) {
      default_case = stmt->range.jump->destination;
      next_pcode( &stmt->range );
   }
   note->exit = stmt->range.pcode;
   stmt->break_stmt = true;
   stmt->break_obj_pos = note->exit->obj_pos;
   struct stmt_discovery body;
   init_stmt_discovery( &body, stmt,
      note->body_start,
      note->body_end );
   examine_block( discovery, &body );
   if ( default_case ) {
      struct case_note* case_note = mem_alloc( sizeof( *note ) );
      init_note( &case_note->note, NOTE_CASE );
      case_note->value = 0;
      case_note->default_case = true;
      append_note( default_case, &case_note->note );
   }
   if ( note->sorted_jump ) {
      struct casejump_pcode* jump = note->sorted_jump->head;
      while ( jump ) {
         struct case_note* case_note = mem_alloc( sizeof( *note ) );
         init_note( &case_note->note, NOTE_CASE );
         case_note->value = jump->value;
         case_note->default_case = false;
         append_note( jump->destination, &case_note->note );
         jump = jump->next;
      }
   }
   else if ( note->case_start ) {
      struct pcode_range range;
      init_range( &range, note->case_start, note->case_end );
      while ( have_pcode( &range ) ) {
         struct case_note* case_note = mem_alloc( sizeof( *note ) );
         init_note( &case_note->note, NOTE_CASE );
         case_note->value = range.casejump->value;
         case_note->default_case = false;
         append_note( range.casejump->destination, &case_note->note );
         next_pcode( &range );
      }
   }
}

static void examine_returnval( struct discovery* discovery,
   struct stmt_discovery* stmt, struct expr_discovery* expr ) {
   struct return_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_RETURN );
   note->expr_start = expr->start;
   note->expr_end = expr->end;
   note->exit = stmt->range.pcode->next;
   append_note( expr->start, &note->note );
   next_pcode( &stmt->range );
}

static void examine_expr_stmt( struct stmt_discovery* stmt,
   struct expr_discovery* expr ) {
   struct expr_stmt_note* note = mem_alloc( sizeof( *note ) );
   init_note( &note->note, NOTE_EXPRSTMT );
   note->expr_start = expr->start;
   note->expr_end = expr->end;
   note->exit = expr->exit;
   append_note( expr->start, &note->note );
   seek_pcode( &stmt->range, note->exit );
   if ( stmt->range.pcode->opcode == PCD_DROP ) {
      note->exit = note->exit->next;
      next_pcode( &stmt->range );
   }
}
