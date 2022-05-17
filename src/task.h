#ifndef SRC_TASK_H
#define SRC_TASK_H

#include <setjmp.h>

#include "common.h"
#include "pcode.h"

struct options {
   const char* object_file;
   const char* source_file;
   bool disassemble;
};

// ==========================================================================

struct acs_object {
   enum {
      FORMAT_UNKNOWN,
      FORMAT_ZERO,
      FORMAT_BIGE,
      FORMAT_LITTLEE
   } format;
   const char* data;
};

struct pcode {
   struct pcode* prev;
   struct pcode* next;
/*
   struct pcode_arg* args;
   union {
      struct {
         struct pcode* destination;
      } jump;
   } more;
*/
   struct note* note;
   i32 opcode;
   i32 obj_pos;
};

// Used by:
// - PCD_GOTO
// - PCD_IFGOTO
// - PCD_IFNOTGOTO
struct jump_pcode {
   struct pcode pcode;
   struct pcode* destination;
   i32 destination_obj_pos;
};

// Used by:
// - PCD_CASEGOTO
struct casejump_pcode {
   struct pcode pcode;
   struct casejump_pcode* next;
   struct pcode* destination;
   i32 destination_obj_pos;
   i32 value;
};

// Used by:
// - PCD_CASEGOTOSORTED
struct sortedcasejump_pcode {
   struct pcode pcode;
   struct casejump_pcode* head;
   struct casejump_pcode* tail;
   i32 count;
};

struct generic_pcode {
   struct pcode pcode;
   struct generic_pcode_arg* args;
   struct generic_pcode_arg* args_tail;
};

struct generic_pcode_arg {
   struct generic_pcode_arg* next;
   i32 value;
};

struct pcode_range {
   struct pcode* start;
   struct pcode* end;
   struct pcode* pcode;
   struct jump_pcode* jump;
   struct casejump_pcode* casejump;
   struct sortedcasejump_pcode* sortedcasejump;
   struct generic_pcode* generic;
};

struct note {
   enum {
      NOTE_IF,
      NOTE_SWITCH,
      NOTE_CASE,
      NOTE_LOOP,
      NOTE_DO,
      NOTE_FOR,
      NOTE_JUMP,
      NOTE_RETURN,
      NOTE_EXPRSTMT,
      NOTE_INTERNFUNC,
   } type;
   struct note* next;
};

struct if_note {
   struct note note;
   struct pcode* cond_start;
   struct pcode* cond_end;
   struct pcode* body_start;
   struct pcode* body_end;
   struct pcode* else_body_start;
   struct pcode* else_body_end;
   struct pcode* exit;
};

struct switch_note {
   struct note note;
   struct pcode* cond_start;
   struct pcode* cond_end;
   struct pcode* body_start;
   struct pcode* body_end;
   struct pcode* case_start;
   struct pcode* case_end;
   struct sortedcasejump_pcode* sorted_jump;
   struct pcode* exit;
};

struct case_note {
   struct note note;
   i32 value;
   bool default_case;
};

struct loop_note {
   struct note note;
   struct pcode* cond_start;
   struct pcode* cond_end;
   struct pcode* body_start;
   struct pcode* body_end;
   struct pcode* exit;
   bool until;
};

struct do_note {
   struct note note;
   struct pcode* cond_start;
   struct pcode* cond_end;
   struct pcode* body_start;
   struct pcode* body_end;
   struct pcode* exit;
   bool until;
};

struct for_note {
   struct note note;
   struct pcode* cond_start;
   struct pcode* cond_end;
   struct pcode* post_start;
   struct pcode* post_end;
   struct pcode* body_start;
   struct pcode* body_end;
   struct pcode* exit;
   struct list post;
};

struct for_note_post {
   struct pcode* start;
   struct pcode* end;
};

struct jump_note {
   struct note note;
   enum {
      JUMPNOTE_BREAK,
      JUMPNOTE_CONTINUE
   } stmt;
};

struct return_note {
   struct note note;
   struct pcode* expr_start;
   struct pcode* expr_end;
   struct pcode* exit;
};

struct expr_stmt_note {
   struct note note;
   struct pcode* expr_start;
   struct pcode* expr_end;
   struct pcode* exit;
};

struct intern_func_note {
   struct note note;
   struct func* func;
   struct pcode* exit;
};

// ==========================================================================

struct node {
   enum {
      // 0
      NODE_NONE,
      NODE_LITERAL,
      NODE_UNARY,
      NODE_BINARY,
      NODE_EXPR,
      NODE_INDEXEDSTRINGUSAGE,
      NODE_JUMP,
      NODE_SCRIPTJUMP,
      NODE_IF,
      NODE_WHILE,
      // 10
      NODE_FOR,
      NODE_CALL,
      NODE_FUNC,
      NODE_ACCESS,
      NODE_PAREN,
      NODE_SUBSCRIPT,
      NODE_CASE,
      NODE_CASEDEFAULT,
      NODE_SWITCH,
      NODE_BLOCK,
      // 20
      NODE_GOTO,
      NODE_GOTOLABEL,
      NODE_VAR,
      NODE_ASSIGN,
      NODE_STRUCTURE,
      NODE_STRUCTUREMEMBER,
      NODE_ENUMERATION,
      NODE_ENUMERATOR,
      NODE_CONSTANT,
      NODE_RETURN,
      // 30
      NODE_PARAM,
      NODE_PALTRANS,
      NODE_ALIAS,
      NODE_BOOLEAN,
      NODE_NAMEUSAGE,
      NODE_SCRIPT,
      NODE_EXPRSTMT,
      NODE_CONDITIONAL,
      NODE_STRCPYCALL,
      NODE_LOGICAL,
      // 40
      NODE_INC,
      NODE_FIXEDLITERAL,
      NODE_CAST,
      NODE_INLINEASM,
      NODE_ASSERT,
      NODE_TYPEALIAS,
      NODE_FOREACH,
      NODE_NULL,
      NODE_NAMESPACE,
      NODE_NAMESPACEFRAGMENT,
      // 50
      NODE_UPMOST,
      NODE_CURRENTNAMESPACE,
      NODE_USING,
      NODE_MEMCPY,
      NODE_CONVERSION,
      NODE_SURE,
      NODE_DO,
      NODE_COMPOUNDLITERAL,
      NODE_PCODE,
      NODE_INCPOST,
      // 60
      NODE_ASPEC,
      NODE_UNKNOWN,
   } type;
};

enum {
   SPEC_NONE,
   SPEC_RAW,
   SPEC_INT,
   SPEC_FIXED,
   SPEC_BOOL,
   SPEC_STR,
   SPEC_VOID,
   SPEC_TOTAL,

   SPEC_NAME = SPEC_TOTAL
};

struct literal {
   struct node node;
   i32 value;
};

struct name_usage {
   struct node node;
   const char* name;
};

struct unary {
   struct node node;
   enum {
      UOP_NONE,
      UOP_MINUS,
      UOP_LOGICALNOT,
      UOP_BITWISENOT
   } op;
   struct node* operand;
};

// Used by: NODE_INC, NODE_INCPOST.
struct inc {
   struct node node;
   struct node* operand;
   bool decrement;
};

struct subscript {
   struct node node;
   struct node* lside;
   struct expr* index;
};

struct binary {
   struct node node;
   enum {
      BOP_NONE,
      BOP_LOG_OR,
      BOP_LOG_AND,
      BOP_BIT_OR,
      BOP_BIT_XOR,
      BOP_BIT_AND,
      BOP_EQ,
      BOP_NEQ,
      BOP_LT,
      BOP_LTE,
      BOP_GT,
      BOP_GTE,
      BOP_SHIFT_L,
      BOP_SHIFT_R,
      BOP_ADD,
      BOP_SUB,
      BOP_MUL,
      BOP_DIV,
      BOP_MOD,
      BOP_TOTAL
   } op;
   struct node* lside;
   struct node* rside;
};

struct assign {
   struct node node;
   enum {
      AOP_SIMPLE,
      AOP_ADD,
      AOP_SUB,
      AOP_MUL,
      AOP_DIV,
      AOP_MOD,
      AOP_SHIFT_L,
      AOP_SHIFT_R,
      AOP_BIT_AND,
      AOP_BIT_XOR,
      AOP_BIT_OR,
      AOP_TOTAL
   } op;
   struct node* lside;
   struct node* rside;
};

struct format_item {
   enum {
      FCAST_ARRAY,
      FCAST_BINARY,
      FCAST_CHAR,
      FCAST_DECIMAL,
      FCAST_FIXED,
      FCAST_RAW,
      FCAST_KEY,
      FCAST_LOCAL_STRING,
      FCAST_NAME,
      FCAST_STRING,
      FCAST_HEX,
      FCAST_MSGBUILD,
      FCAST_TOTAL
   } cast;
   struct format_item* next;
   struct expr* value;
   void* extra;
};

struct format_item_array {
   struct expr* offset;
   struct expr* length;
};

struct call {
   struct node node;
   struct node* operand;
   struct format_item* format_item;
   struct list args;
   bool direct;
};

struct strcpy_call {
   struct node node;
   struct expr* array;
   struct expr* array_offset;
   struct expr* array_length;
   struct expr* string;
   struct expr* offset;
};

struct palrange {
   struct expr* begin;
   struct expr* end;
   union {
      struct {
         struct expr* begin;
         struct expr* end;
      } colon;
      struct {
         struct expr* red1;
         struct expr* green1;
         struct expr* blue1;
         struct expr* red2;
         struct expr* green2;
         struct expr* blue2;
      } rgb;
      struct {
         struct expr* red;
         struct expr* green;
         struct expr* blue;
      } colorisation;
      struct {
         struct expr* amount;
         struct expr* red;
         struct expr* green;
         struct expr* blue;
      } tint;
   } value;
   enum {
      PALRANGE_COLON,
      PALRANGE_RGB,
      PALRANGE_SATURATED,
      PALRANGE_COLORISATION,
      PALRANGE_TINT,
   } type;
};

struct paltrans {
   struct node node;
   struct expr* number;
   struct list ranges;
};

struct paren {
   struct node node;
   struct node* contents;
};

struct unknown {
   struct node node;
   enum {
      UNKNOWN_ASPEC,
      UNKNOWN_EXT,
   } type;
   union {
      struct {
         i32 id;
      } aspec;
      struct {
         i32 id;
      } ext;
   } more;
};

struct expr {
   struct node node;
   struct node* root;
   i32 spec;
   // The value is handled based on the expression type. For numeric types,
   // it'll contain the numeric result of the expression. For the string type,
   // it'll contain the string index, that can be used to lookup the string.
   i32 value;
   bool folded;
   bool has_str;
};

struct expr_stmt {
   struct node node;
   struct expr* expr;
};

struct block {
   struct node node;
   struct list stmts;
};

struct jump {
   struct node node;
   enum {
      JUMP_BREAK,
      JUMP_CONTINUE
   } type;
};

struct script_jump {
   struct node node;
   enum {
      SCRIPTJUMP_TERMINATE,
      SCRIPTJUMP_RESTART,
      SCRIPTJUMP_SUSPEND,
      SCRIPTJUMP_TOTAL
   } type;
};

struct if_stmt {
   struct node node;
   struct expr* cond;
   struct block* body;
   struct block* else_body;
};

struct case_label {
   struct node node;
   i32 value;
};

struct switch_stmt {
   struct node node;
   struct expr* cond;
   struct case_label* case_head;
   struct block* body;
};

struct while_stmt {
   struct node node;
   struct expr* cond;
   struct block* body;
   bool until;
};

struct do_stmt {
   struct node node;
   struct expr* cond;
   struct block* body;
   bool until;
};

struct for_stmt {
   struct node node;
   struct expr* cond;
   struct list post;
   struct block* body;
};

struct return_stmt {
   struct node node;
   struct expr* return_value;
};

struct inline_asm {
   struct node node;
   struct pcode* pcode;
};

struct initial {
   struct initial* next;
   bool multi;
};

struct value {
   struct initial initial;
   struct value* next;
   enum {
      VALUE_NUMBER,
      VALUE_STRING,
      VALUE_FUNC
   } type;
   i32 index;
   i32 value;
};

struct multi_value {
   struct initial initial;
   struct initial* body;
};

struct var {
   struct node node;
   struct str name;
   struct expr* initz;
   struct initial* initial;
   struct value* value;
   enum storage {
      STORAGE_LOCAL,
      STORAGE_MAP,
      STORAGE_WORLD,
      STORAGE_GLOBAL
   } storage;
   u32 dim_length;
   u32 index;
   u32 spec;
   bool array;
   bool imported;
   bool used;
   bool declared;
};


struct param {
   i32 spec;
};

struct func {
   struct node node;
   enum {
      FUNC_ASPEC,
      FUNC_DED,
      FUNC_EXT,
      FUNC_FORMAT,
      FUNC_USER,
      FUNC_INTERN,
   } type;
   struct str name;
   union {
      struct func_aspec* aspec;
      struct func_ext* ext;
      struct func_ded* ded;
      struct func_format* format;
      struct func_user* user;
      struct func_intern* intern;
   } more;
   struct list params;
   i32 return_spec;
   i32 min_param;
   i32 max_param;
};

struct func_aspec {
   i32 id;
};

// An extension function doesn't have a unique pcode reserved for it, but is
// instead called using the PC_CALL_FUNC pcode.
struct func_ext {
   i32 id;
};

// A function with an opcode allocated to it.
struct func_ded {
   i32 opcode;
};

// Format functions are dedicated functions and have their first parameter
// consisting of a list of format items.
struct func_format {
   enum pcd opcode;
};

struct aspec {
   struct node node;
   const char* name;
};

// Functions created by the user.
struct func_user {
   struct pcode* start;
   struct pcode* end;
   struct block* body;
   struct var** vars;
   struct var** arrays;
   u32 offset;
   u32 end_offset;
   u32 index;
   u32 num_vars;
/*
   struct list labels;
   struct block* body;
   struct func* next_nested;
   struct func* nested_funcs;
   struct call* nested_calls;
   struct return_stmt* returns;
   struct c_point* prologue_point;
   struct c_sortedcasejump* return_table;
   struct list vars;
   struct list funcscope_vars;
   int index;
   int size;
   int usage;
   int obj_pos;
   enum {
      RECURSIVE_UNDETERMINED,
      RECURSIVE_POSSIBLY
   } recursive;
   bool nested;
   bool local;
*/
};

struct func_intern {
   enum {
      INTERNFUNC_ACSEXECUTEWAIT,
      INTERNFUNC_ACSNAMEDEXECUTEWAIT,
      INTERNFUNC_TOTAL
   } id;
};

#define SCRIPTFLAG_NET 0x1u
#define SCRIPTFLAG_CLIENTSIDE 0x2u

struct script {
   struct node node;
   struct pcode* body_start;
   struct pcode* body_end;
   struct var** vars;
   struct var** arrays;
   struct str name;
   i32 number;
   u32 offset;
   u32 end_offset;
   u32 num_param;
   enum script_type {
      SCRIPT_TYPE_CLOSED,
      SCRIPT_TYPE_OPEN,
      SCRIPT_TYPE_RESPAWN,
      SCRIPT_TYPE_DEATH,
      SCRIPT_TYPE_ENTER,
      SCRIPT_TYPE_PICKUP,
      SCRIPT_TYPE_BLUERETURN,
      SCRIPT_TYPE_REDRETURN,
      SCRIPT_TYPE_WHITERETURN,
      SCRIPT_TYPE_LIGHTNING = 12,
      SCRIPT_TYPE_UNLOADING,
      SCRIPT_TYPE_DISCONNECT,
      SCRIPT_TYPE_RETURN,
      SCRIPT_TYPE_EVENT,
      SCRIPT_TYPE_KILL,
      SCRIPT_TYPE_REOPEN,
      SCRIPT_TYPE_NEXTFREENUMBER,
      SCRIPT_TYPE_TOTAL = SCRIPT_TYPE_NEXTFREENUMBER
   } type;
   u32 flags;
   u32 num_vars;
   struct block* body;
   bool named_script;
};

struct imported_module {
   struct str name;
};

#define DIAG_NONE 0
#define DIAG_WARN 0x8
#define DIAG_ERR 0x10
#define DIAG_INTERNAL 0x40
#define DIAG_NOTE 0x80

struct task {
   jmp_buf bail;
   struct options* options;
   struct {
      struct pcode* head;
      struct pcode* tail;
      struct pcode* current;
      struct generic_pcode_arg* args_tail;
   } pcode_alloc;
   struct str library_name;
   struct list objects;
   struct list scripts;
   struct list funcs;
   struct list imports;
   struct var* map_vars[ 128 ];
   struct var* world_vars[ 256 ];
   struct var* world_arrays[ 256 ];
   struct var* global_vars[ 64 ];
   struct var* global_arrays[ 64 ];
   struct func** ded_funcs;
   struct func** format_funcs;
   struct func** ext_funcs;
   struct func** intern_funcs;
   struct str* strings;
   u32 num_strings;
   bool encrypt_str;
   bool importable;
   bool compact;
   bool wadauthor;
   bool calls_aspec;
   bool calls_ext;
};

void t_create_builtins( struct task* task );
void t_load( struct task* task );
void t_show( struct task* task );
void t_annotate( struct task* task );
void t_recover( struct task* task );
void t_publish( struct task* task );
void t_diag( struct task* task, i32 flags, ... );
void t_diag_args( struct task* task, i32 flags, va_list* args );
void t_bail( struct task* task );
struct func* t_alloc_func( void );

struct pcode_info* c_get_pcode_info( enum pcd opcode );
const struct direct_pcode_info* c_get_direct_pcode( enum pcd opcode );
struct func* t_get_ded_func( struct task* task, i32 opcode );
struct func* t_find_format_func( struct task* task, i32 opcode );
struct func* t_find_intern_func( struct task* task, i32 opcode );
struct func* t_find_ext_func( struct task* task, i32 id );
bool t_is_direct_pcode( i32 opcode );
const char* t_lookup_string( struct task* task, u32 index );
struct script* t_find_script( struct task* task, i32 number );
struct func* t_find_func( struct task* task, u32 index );
struct var* t_reserve_map_var( struct task* task, u32 index );
struct expr* t_alloc_literal_expr( i32 value );
struct var* t_alloc_var( void );
struct param* t_alloc_param( void );
bool t_uses_zcommon_file( struct task* task );
void t_analyze( struct task* task );

#endif
