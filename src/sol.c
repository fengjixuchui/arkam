#include "arkam.h"
#include "shorthands.h"
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>

// ===== Library =====
#include <core.sol.h>


// ===== Config =====

#define TOKEN_BUF_LEN 2048
#define NEST_SEPARATOR ':'

// ===== Usage =====

char* USAGE =
  "Usage: sol [options] SOURCES IMAGE\n"
  "Options:\n"
  "    -n, --no-corelib  Not to load core library\n"
  "    -h, --help        Show this help\n"
  "Example:\n"
  "    sol main.sol app.img\n"
  "    sol -n lib.sol main.sol app.img\n"
  ;


/* ===== Memory Layout =====
   0x00 | DO NOT ACCESS
   0x04 | Startup Routine
   0x08 | reserved
   0x0C | reserved
   0x10 | CODE...
*/

#define CODE_BEGIN_CELL 4

/* ===== Dictionary Layout =====
   (toplevel) -> current -> next
                 |-> child -> ... -> parent-next

   In colon definition, a word should be searched from the latest child of
   the current definition
   if it has children. Otherwise from next of the current definition.

   This layout is for the nested hyper-static environment.

   example:
   ```
     : foo 42 ;
     : foo
       : bar foo ;
       : bar 
         : baz foo ;
         bar ;
       foo ;
   ```
   foo, foo:bar, and foo:bar:baz should returns 42 by first definition of foo.
   All of calling foo in second foo definition should call the first foo.
   They should not recurse infinitely.

   Also the same word with no child definition should hide all of the previous ones.

   ```
     : foo : bar ; ;
     : foo ( children can be here ) ;
     : main foo:bar ; ( not found )
   ```
*/


// ===== Structs =====

typedef struct Word Word;
typedef struct Context Context;

typedef void(*WordHandler)(Context* ctx, Word* word);

typedef enum { WordPrim, WordUser, WordQuot, WordConst, WordVal } WordType;

struct Word {
  WordType     type;
  char*        name;
  WordHandler  handler;
  Word*        next;
  Word*        child;   // child dictionary
  Word*        parent;  // current defining word chain
  int          level;   // nested level  
  int          inst;
  Cell         back;    // back patching address for control flow
};

typedef struct Source {
  char* fname;
  char* text;
  int   compiling; // circular
  struct Source* next;
} Source;

struct Context {
  ArkamVM* vm;
  Cell     start;         // entry point address
  Cell     here;
  char*    p;             // compiler current position
  char*    token_buf;
  int      token_buf_len; // actual size - 1
  Source*  source;        // reverse ordered link list
  char*    source_name;   // current source name
  Source*  includes;      // included files
  char*    image_name;
  FILE*    image_file;
  Word*    dict;
  Word*    current;       // current defining word
  int      search_level;
};

typedef struct SolOption {
  int use_corelib;
} SolOption;


// ===== Forward Declarations =====
Word* find_word(Context* ctx, char* name);
int read_number(Context* ctx, Cell* result);
char* read_source(char* fname);
void compile_source(Context* ctx);


// Debug print for internal use
// =============================================================================

#if defined(__GNUC__)
#  define DebugAid __attribute__((unused)) static
#else
#  define DebugAid static
#endif

DebugAid void debug_print(char* filename, int line, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  fprintf(stderr, "\e[31m");
  fprintf(stderr, "%s:%d ", filename, line);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\e[0m\n");
}

#define DBG(...) { debug_print(__FILE__, __LINE__, __VA_ARGS__); }
#define DIE(...) { DBG(__VA_ARGS__) exit(1); }


// Shorthands
// =============================================================================
#define Put(v)  (put(ctx, (v)))
#define PutN(n) (putn(ctx, (n)))
#define PutI(i) (put(ctx, (ARK_INST_##i << 1) | 0x01))
#define PutB(v) (putb(ctx, (v)))


// Align
#define ALIGN_CELL (sizeof(Cell) - 1)
Cell align(Cell n) {
  return (n + ALIGN_CELL) & ~ALIGN_CELL;
}


// Die
// =============================================================================

void die(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void set_file_pos(Context* ctx, int* dst_line, int* dst_col) {
  int line = 0;
  int col  = 0;
  char* p   = ctx->source->text;
  char* end = ctx->p;

  while (*p != '\0' && p < end) {
    char c = *p;
    p++;
    if (c != '\n') {
      col++;
      continue;
    }
    line++;
    col = 0;
  }

  *dst_line = line;
  *dst_col  = col;
}

void die_at(Context* ctx, char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int line = 0;
  int col  = 0;
  set_file_pos(ctx, &line, &col);

  fprintf(stderr, "ERROR %s(%d:%d) ", ctx->source_name, line, col);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);  
}


// Dictionary
// =============================================================================

Word* new_word(WordType type) {
  Word* word = calloc(sizeof(Word), 1);
  word->type = type;
  return word;
}

Word* create_dict_entry(Context* ctx, const char* cname) {
  Word* parent = ctx->current;

  if (parent && parent->inst != ctx->here)
    die_at(ctx, "Nested word %s is not at first of parent definition", cname);
  if (parent && parent->type == WordQuot)
    die_at(ctx, "Do not create nested word in quotation");
    
  int   len  = strlen(cname) + 1;
  char* name = calloc(sizeof(char), len);
  strcpy(name, cname);

  Word* word = new_word(WordUser);
  word->name = name;
  
  // Push current definition stack
  word->parent = parent;
  ctx->current = word;

  // handle nested word definition
  if (parent) {
    word->level = parent->level + 1;
    // nested, connect to child link
    word->next = parent->child ? parent->child : parent->next;
    parent->child = word;
  } else {
    word->level = 0;
    // toplevel, connect to root link
    word->next = ctx->dict;
    ctx->dict  = word;
  }
  return word;
}

void free_dict(Word* word) {
  while(word) {
    Word* now = word;
    word = word->child ? word->child : word->next;
    if (now->type == WordQuot)
      die("Quotation entry remains dictionary");    
    free(now->name);
    free(now);
  }
}


// Dump
// =============================================================================

void dump(Context* ctx) {
  VM* vm = ctx->vm;
  
  DBG("-----MEM-----");
  for (int i = 0; Cells(i) < ctx->here; i++) {
    Cell addr = Cells(i);
    DBG("%03x(% 4d) | %03x(% 4d)", addr, addr, Get(addr), Get(addr));
  }
}


// Builder
// =============================================================================

void put(Context* ctx, Cell v) {
  VM* vm = ctx->vm;
  Set(ctx->here, v);
  ctx->here += Cells(1);
}

void putn(Context* ctx, Cell n) {
  PutI(LIT);
  put(ctx, n);
}

void putb(Context* ctx, Byte v) {
  // put a byte
  VM* vm = ctx->vm;
  Set(ctx->here, v);
  ctx->here += 1;
}


// Source and Image file
// =============================================================================

FILE* open_file(char* desc, char* fname, char* mode) {
  FILE* file = fopen(fname, mode);
  if (!file) {
    perror("file");
    die("%s(%s): %s", strerror(errno), desc, fname);
  }

  return file;
}

Source* free_source(Source* src) {
  if (!src) return NULL;
  Source* next = src->next;
  free(src->text);
  free(src);
  return next;
}

Source* add_source(Context* ctx, char* fname, char* text) {
  Source* src = calloc(sizeof(Source), 1);
  if (!src) die("Can't allocate source for %s", fname);

  src->fname = fname;
  src->text  = text;

  src->next = ctx->source;
  ctx->source = src;
  return src;
}

Source* reverse_source_inplace(Source* src) {
  Source* before = NULL;
  Source* next = NULL;
  while (src) {
    next = src->next;
    src->next = before;
    before = src;
    src = next;
  }
  return before;
}

void add_corelib(Context* ctx) {
  char* text = calloc(sizeof(core_lib), 1);
  if (!text) die("Can't allocate corelib source");
  strcpy(text, core_lib);
  add_source(ctx, "core library", text);
}

char* read_source(char* fname) {
  FILE* file = open_file("source", fname, "r");

  // get file size
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  rewind(file);

  // allocate
  char* source = calloc(sizeof(char), size + 1);
  if (!source) die("Can't allocate buffer for source (size:%d)", size+1);

  // read
  int read = fread(source, 1, size, file);
  if (read < size) die("%s (source): %s", strerror(errno), fname);
  source[size] = '\0';

  fclose(file);
  
  return source;
}

Cell read_blob(VM* vm, Cell addr, char* fname) {
  /* return size */
  FILE* file = open_file("blob", fname, "rb");

  // get file size
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  rewind(file);

  if (!ark_valid_addr(vm, addr + size))
    die("Can't read %s: too big", fname);
  

  Byte* dst = vm->mem + addr;
  // read
  int read = fread(dst, 1, size, file);
  if (read < size) die("%s (blob): %s", strerror(errno), fname);
  dst[size] = '0';

  fclose(file);
  
  return size;
}


void open_source(Context* ctx, char* fname) {
  char* src = read_source(fname);
  add_source(ctx, fname, src);
}

void open_image(Context* ctx, char* fname) {
  ctx->image_name = fname;
  ctx->image_file = open_file("image", fname, "wb");
}


// Token Reader
// =============================================================================

int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void skip_spaces(Context* ctx) {
  while (is_space(*ctx->p)) {
    ctx->p++;
  }
}

int read_token(Context* ctx) {
  // returns 0 if there is no more token
  skip_spaces(ctx);

  char* start = ctx->p;
  int   i    = 0;
  int   max  = ctx->token_buf_len;
  char  c    = *ctx->p;

  if (c == '\0') return 0;
  
  while (c != '\0' && !is_space(c)) {
    if (max <= i) {
      ctx->p = start;
      die_at(ctx, "Too long token");
    }

    ctx->token_buf[i] = c;
    i++;
    ctx->p++;
    c = *ctx->p;
  }
  
  ctx->token_buf[i] = '\0';
  return 1;
}

int read_digit(char c, int base) {
  // return a number or -1 if c is not a digit
  
  // decimal
  if (c >= '0' && c <= '9') return c - '0';
  if (base == 10) return -1;
  // hex
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}


// Word Handlers
// =============================================================================

void handle_inst(Context* ctx, Word* word) {
  Put(word->inst);
}

void handle_colon(Context* ctx, Word* word) {
  if (ctx->here != align(ctx->here)) die_at(ctx, "Not aligned word");
  if (read_token(ctx) == 0) die_at(ctx, "Word name required");

  Word* entry = create_dict_entry(ctx, ctx->token_buf);
  entry->handler = handle_inst;
  entry->inst    = ctx->here;
}

void close_nest(Context* ctx) {
  // Pop current definition stack
  Word* parent = ctx->current->parent;
  ctx->current = parent;
  
  // If in nested word back patch start of parent
  if (parent) {
    parent->inst = ctx->here;
  }  
}

void handle_semicolon(Context* ctx, Word* word) {
  if (!ctx->current) die_at(ctx, "Semicolon out of word definition");

  PutI(RET);

  close_nest(ctx);
}


/* ----- defconst / const ----- */

void handle_const(Context* ctx, Word* word) {
  // Put `lit inst`
  PutI(LIT);
  Put(word->inst);
}

void read_const(Context* ctx, Word* entry) {
  if (read_token(ctx) == 0) die_at(ctx, "Constant value required");
  const char* err = "Constant value should be number or constant";
  
  Word* found = find_word(ctx, ctx->token_buf);
  if (found) {
    if (found->type != WordConst) die_at(ctx, "%s", err);
    entry->inst = found->inst;
    return;
  }

  Cell n;
  if (!read_number(ctx, &n)) die_at(ctx, "%s", err);
  entry->inst = n;
}

void handle_defconst(Context* ctx, Word* word) {
  if (read_token(ctx) == 0) die_at(ctx, "Constant name required");

  Word* entry = create_dict_entry(ctx, ctx->token_buf);

  read_const(ctx, entry);
  entry->type    = WordConst;
  entry->handler = handle_const;

  close_nest(ctx);
}



/* ----- defval -----
   example:
     val: x

   creates words
     : x  lit ADDR1 @ ;
     : x! lit ADDR2 ! ;
   Cell at ADDR1 is set to 0 for end of link list.
   Cell at ADDR2 is set to address of ADDR1 for back patching.
   Word entry of x has back patching address to ADDR2.
   entry -> ADDR2 -> ADDR1(0)

   Backpatching should insert a same heap address to ADDR1 and ADDR2.
*/

void handle_defval(Context* ctx, Word* word) {
  if (read_token(ctx) == 0) die_at(ctx, "Word name required");
  int len = strlen(ctx->token_buf);
  if (len + 1 >= TOKEN_BUF_LEN) die_at(ctx, "Too long val name: %s", ctx->token_buf);
  ctx->token_buf[len+1] = '\0'; // for val! and val&

  
  // word: valname
  // LIT 0 @ RET
  Word* entry = create_dict_entry(ctx, ctx->token_buf);
  entry->type    = WordVal;
  entry->handler = handle_inst;
  entry->inst    = ctx->here;
  entry->back    = 0; // for backpatching

  PutI(LIT);
  entry->back = ctx->here;
  Put(0);
  PutI(GET);

  handle_semicolon(ctx, entry);

  
  // word: valname!
  // LIT 0 ! RET
  ctx->token_buf[len] = '!';
  Word* setter = create_dict_entry(ctx, ctx->token_buf);
  setter->type    = WordUser;
  setter->handler = handle_inst;
  setter->inst    = ctx->here;

  PutI(LIT);
  Cell back = entry->back;
  entry->back = ctx->here;
  Put(back);
  PutI(SET);
  PutI(RET);

  handle_semicolon(ctx, entry);
}


/* ----- if/else -----
   `if` and `else` pushes back patching address to stack of vm.
   `else` and `end` do back patching.
   `if foo end bar` will be compiled to
           | 0jmp
           | &end
           | foo
      end: | bar

   `if foo else bar end baz` will be compiled to
           | 0jmp
           | &else
           | foo
           | jmp
           | &end
     else: | bar
      end: | baz
*/

void handle_if(Context* ctx, Word* word) {
  VM* vm = ctx->vm;
  PutI(ZJMP);
  Push(ctx->here); // for back patching
  Put(0); // tmporary
}

void handle_else(Context* ctx, Word* word) {
  VM* vm = ctx->vm;

  // put jump for if-else block
  PutI(JMP);
  Cell back = ctx->here;
  Put(0); // temporary
  
  // swap back patch address
  Cell addr = Pop();
  Push(back);

  // back patch
  Set(addr, ctx->here);
}

void handle_end(Context* ctx, Word* word) {
  VM* vm = ctx->vm;
  Cell addr = Pop();
  Set(addr, ctx->here);
}


/* ----- again/recur -----
   again is tail recursion. recur is not.
   again will be compiled to `jmp &latest`.
   recur will be simply compiled to `&latest`.
*/

void handle_again(Context* ctx, Word* word) {
  Word* latest = ctx->current;
  if (!latest) die("Using again out of word");
  PutI(JMP);
  Put(latest->inst);
}

void handle_recur(Context* ctx, Word* word) {
  Word* latest = ctx->current;
  if (!latest) die("Using recur out of word");
  Put(latest->inst);  
}


// ----- comment -----

void handle_comment(Context* ctx, Word* word) {
  char c = *ctx->p;
  while (c != '\0' && c != '\n') {
    ctx->p++;
    c = *ctx->p;
  }
}

void handle_paren(Context *ctx, Word* word) {
  char* start = ctx->p;
  char c = *ctx->p;
  while (c != ')') {
    if (c == '\0') {
      ctx->p = start;
      die_at(ctx, "Unclosed paren");
    }
    ctx->p++;
    c = *ctx->p;
  }
  ctx->p++; // skip ')'
}


/* ----- quotation -----
  (BEFORE) [ ... ] (AFTER) => jmp &AFTER ... ret lit &BEFORE
  open_quot pushes QuotationWord to ctx->current
  close_quot pops and free it and do backpatching.
*/

void handle_open_quot(Context* ctx, Word* word) {
  Word* quot = new_word(WordQuot);
  if (!quot) die_at(ctx, "Can't allocate quotation");
  Word* parent = ctx->current;
  if (!parent) die_at(ctx, "Quotation out of definition");
  quot->parent = parent;
  quot->next   = parent->child ? parent->child : parent->next;
  ctx->current = quot;
  
  PutI(JMP);
  quot->back = ctx->here; // back patching addr
  Put(0); // temporary
  quot->inst = ctx->here;
}

void handle_close_quot(Context* ctx, Word* word) {
  VM* vm = ctx->vm;

  Word* quot = ctx->current;
  if (!quot || quot->type != WordQuot)
    die_at(ctx, "Close quot out of quotation");
  ctx->current = quot->parent;
  
  PutI(RET);
  Set(quot->back, ctx->here);
  PutI(LIT);
  Put(quot->inst);
  
  free(quot);
}


void handle_amp(Context* ctx) {
  // &foo => lit &foo
  char* name = ctx->token_buf + 1; // skip first &
  
  Word* found = find_word(ctx, name);
  if (!found) die_at(ctx, "Word not found: %s", name);

  // handle const
  if (found->type == WordConst)
    die_at(ctx, "Word %s is a constant. Do not use & for it.", found->name);

  // handle val
  if (found->type == WordVal) {
    PutI(LIT);
    Cell back = found->back;
    found->back = ctx->here;
    Put(back);
    return;
  }

  // normal
  PutI(LIT);
  Put(found->inst);
}


/* ----- include ----- */

char* read_include_path(Context* ctx) {
  char* name;
  int len = 0;

  ctx->p++; // skip first double quote
  char* first = ctx->p;
  
  while (*ctx->p && *ctx->p != '"') {
    len++;
    ctx->p++;
  }

  if (*ctx->p == '\0') die_at(ctx, "unclosed filename");
  ctx->p++; // skip last double quote
  
  name = calloc(sizeof(char), len+1);
  memcpy(name, first, len);
  name[len] = '\0';
  
  return name;
}

char* full_path(char* fname) {
  #ifdef __MINGW32__
  return _fullpath(NULL, fname, _MAX_PATH);
  #else
  return realpath(fname, NULL);
  #endif
}

void fix_path(char* path) {
#ifdef __MINGW32__
  const char ng = '/';
  const char ok = '\\';
#else
  const char ng = '\\';
  const char ok = '/';
#endif
  char* p = path;
  while (*p != '\0') {
    if (*p == ng) *p = ok;
    p++;
  }
}

char* path_in_dir(char* rel, char* fname) {
#ifdef __MINGW32__
  const char sep = '\\';
#else
  const char sep = '/';
#endif

  int flen = strlen(fname);
  
  char* full = full_path(rel);
  char* dir = dirname(full);
  int dlen = strlen(dir);
  
  int len = flen + 1 /* sep */ + dlen;
  char* path = calloc(sizeof(char), len+1);
  
  memcpy(path, dir, dlen);
  path[dlen] = sep;
  memcpy(path+dlen+1, fname, flen);
  path[len] = '\0';

  free(full);
  char* real = full_path(path);
  free(path);
  return real;
}

char* search_source(char* fname, char* rel) {
  fix_path(rel);

  char* full = path_in_dir(rel, fname);
  if (full && access(full, F_OK) == 0) {
    return full;
  }
  free(full);

  char* lib = path_in_dir("lib/core.sol", fname); // if use "lib" you'll get parent of lib
  if (lib && access (lib, F_OK) == 0) {
    return lib;
  }
  free(lib);
  
  return NULL;
}

char* read_source_path(Context* ctx, char* desc) {
  skip_spaces(ctx);

  char* fname;
  if (*ctx->p == '"') fname = read_include_path(ctx);
  else die_at(ctx, "%s name required", desc);

  char* path = search_source(fname, ctx->source_name);
  if (!path) die_at(ctx, "%s not found: %s", desc, fname);

  return path;
}

int already_included(Context* ctx, char* path) {
  Source* s = ctx->includes;
  while (s) {
    if (strcmp(s->fname, path) == 0) {
      if (s->compiling)
        die_at(ctx, "Circular include detected on %s and %s", path, ctx->source_name);
      return 1;
    }
    s = s->next;
  }
  return 0;
}

Source* add_included(Context* ctx, char* path) {
  Source* s = calloc(sizeof(Source), 1);
  s->fname = path;
  s->next = ctx->includes;
  ctx->includes = s;
  return s;
}

void handle_include(Context* ctx, Word* word) {
  skip_spaces(ctx);

  char* fname;
  if (*ctx->p == '"') fname = read_include_path(ctx);
  else die_at(ctx, "no include name");

  char* path = search_source(fname, ctx->source_name);
  if (!path) die_at(ctx, "include: not found: %s", fname);

  if (already_included(ctx, path)) {
    free(path);
    return;
  }

  Source* s = add_included(ctx, path);
  s->compiling = 1; // for circular compiling
 
  char* src = read_source(path);
  char* old_p      = ctx->p;
  char* old_source = ctx->source_name;

  ctx->p = src;
  ctx->source_name = fname;
  compile_source(ctx);
  ctx->p = old_p;
  ctx->source_name = old_source;

  s->compiling = 0;
  // DO NOT FREE PATH, It is used for included list
  free(src);
}


/* ===== datafile ===== */

void handle_datafile(Context* ctx, Word* word) {
  VM* vm = ctx->vm;
  if (read_token(ctx) == 0) die_at(ctx, "datafile: var name required");

  if (ctx->here != align(ctx->here))
    die("datafile: not aligned before");

  /* read var name */
  Word* entry = create_dict_entry(ctx, ctx->token_buf);

  entry->type    = WordConst;
  entry->handler = handle_const;
  entry->inst    = ctx->here;

  /* read file name */
  Cell size_addr = ctx->here;
  Put(0);
  char* path = read_source_path(ctx, "datafile:");
  Cell size = read_blob(vm, ctx->here, path);
  ctx->here = align(ctx->here + size);
  Set(size_addr, size);

  close_nest(ctx);  
}


#define InstOf(str, code) {                                             \
    .name = str, .handler = handle_inst,                                \
      .inst = (ARK_INST_##code << 1) | 0x01 ,                           \
      .next = NULL, .type = WordPrim                                    \
      }

#define PrimOf(str, fn) {                       \
    .name = str, .handler = fn, .next = NULL,   \
      .type = WordPrim                          \
      }

Word word_table[] =
  { InstOf("noop", NOOP),
    InstOf("HALT", HALT),
    InstOf("lit",  LIT),
    InstOf("RET",  RET),
    // Stack
    InstOf("dup",  DUP),
    InstOf("drop", DROP),
    InstOf("swap", SWAP),
    InstOf("over", OVER),
    // Arithmetics
    InstOf("+",    ADD),
    InstOf("-",    SUB),
    InstOf("*",    MUL),
    InstOf("/mod", DMOD),
    // Compare
    InstOf("=",  EQ),
    InstOf("!=", NEQ),
    InstOf(">",  GT),
    InstOf("<",  LT),
    // Control flow
    InstOf("jmp",  JMP),
    InstOf("0jmp", ZJMP),
    // Memory
    InstOf("@",  GET),
    InstOf("!",  SET),
    InstOf("b@", BGET),
    InstOf("b!", BSET),
      // Bitwise
    InstOf("bit-and",    AND),
    InstOf("bit-or",     OR),
    InstOf("bit-not",    NOT),
    InstOf("bit-xor",    XOR),
    InstOf("bit-lshift", LSHIFT),
    InstOf("bit-ashift", ASHIFT),
    // Peripheral
    InstOf("io",  IO),
    // Return stack
    InstOf(">r",     RPUSH),
    InstOf("r>",     RPOP),
    InstOf("rdrop",  RDROP),
    // Registers
    InstOf("sp",  GETSP),
    InstOf("sp!", SETSP),
    InstOf("rp",  GETRP),
    InstOf("rp!", SETRP),
    
    // ===== primitives =====
    PrimOf(":",      handle_colon),
    PrimOf(";",      handle_semicolon),
    PrimOf("const:", handle_defconst),
    PrimOf("val:",   handle_defval),
    PrimOf("IF",     handle_if),
    PrimOf("ELSE",   handle_else),
    PrimOf("END",    handle_end),
    PrimOf("AGAIN",  handle_again),
    PrimOf("RECUR",  handle_recur),
    PrimOf("#",      handle_comment),
    PrimOf("(",      handle_paren),
    PrimOf("[",      handle_open_quot),
    PrimOf("]",      handle_close_quot),

    // ===== directives =====
    PrimOf("include:",  handle_include),
    PrimOf("datafile:", handle_datafile),
  };


Word* find_word_from(Context* ctx, Word* start, char* name);

Word* test_word_name(Word* word, char* name, char** ret_child_name) {
  // Returns word if it has same name.
  // Set child-name if word name is prefixed of passed name.
  // Otherwise returns NULL.
  if (word->type == WordQuot) return NULL; // ignore quotation
  
  char* p = word->name;

  while (!(*p == '\0' && *name == '\0')) {
    // prefixed 
    if (*p == '\0' && *name == NEST_SEPARATOR) {
      *ret_child_name = name+1;
      return NULL;
    }
    // diff
    if (*p != *name) return NULL;
    p++;
    name++;
  }

  // same
  return word;
}

Word* find_in_dict(Context* ctx, Word* word, char* name) {
  Word* found = NULL;
  while (word) {
    // Guard for not searching for child-name in outer level.
    if (word->level < ctx->search_level) return NULL;
    
    char* child_name = NULL;
    found = test_word_name(word, name, &child_name);
    if (found) return found;

    if (child_name) {
      ctx->search_level++;
      return find_in_dict(ctx, word->child, child_name);
    }
    
    word = word->next;
  }
  return NULL;
}

Word* find_word_from(Context* ctx, Word* start, char* name) {
  // search in current dictionary
  Word* word = find_in_dict(ctx, start, name);
  if (word) return word;
  
  // search in instruction/primitive table
  int len = sizeof(word_table) / sizeof(Word);
  for (int i = 0; i < len; i++) {
    Word* word = &word_table[i];
    if (strcmp(word->name, name) != 0) continue;
    return word;
  }

  return NULL;
}

Word* find_word(Context* ctx, char* name) {
  /* search order
     1. current-child
     2. current-next
     3. toplevel
  */
  Word* cur = ctx->current;
  ctx->search_level = 0;
  if (cur)
    return find_word_from(ctx, cur->child ? cur->child : cur->next, name);
  
  return find_word_from(ctx, ctx->dict, name);
}


// Compile
// =============================================================================

int read_number(Context* ctx, Cell* result) {
  int   acc  = 0;
  int   sign = 1;
  char* p    = ctx->token_buf;
  char  c    = *p;
  int   base = 10;

  // negative?
  if (c == '-' && p[1] != '\0') {
    sign = -1;
    p++;
    c = *p;
  }

  // hex?
  if (c == '0' && p[1] == 'x' && p[2] != '\0') {
    base = 16;
    p += 2;
    c = *p;
  }
  
  while (c != '\0') {
    int n = read_digit(c, base);
    if (n < 0) return 0;
    acc *= base;
    acc += n;
    p++;
    c = *p;
  }

  *result = acc * sign;
  return 1;
}

int compile_number(Context* ctx) {
  int n = 0;
  if (!read_number(ctx, &n)) return 0;

  PutN(n);
  return 1;
}

void compile_token(Context* ctx) {
  if (read_token(ctx) == 0) return; // no more token

  if (ctx->token_buf[0] == '&')
    return handle_amp(ctx);

  Word* word = find_word(ctx, ctx->token_buf);
  if (word) return word->handler(ctx, word);
  
  if (compile_number(ctx)) return;

  die_at(ctx, "Unknown token %s", ctx->token_buf);
}

int compile_string(Context* ctx) {
  /* return whether string is parsed or not.
     compile to: jmp &HERE (STR) [string...\0] (HERE) lit &STR
  */

  if (*ctx->p != '"') return 0;

  ctx->p++;
  VM* vm = ctx->vm;
  
  PutI(JMP);
  Cell back = ctx->here;
  Put(0); // temporary
  Cell str = ctx->here;

  char c = *ctx->p;
  while (c != '"') {
    if (c == '\0') die("unterminated string");
    
    if (c != '\\') {
      PutB(c);
      ctx->p++;
      c = *ctx->p;
      continue;
    }
    
    //TODO: handle escape sequence
    ctx->p++;
    c = *ctx->p;
    if (c == '\0') die("unterminated string");
    switch (c) {
    case 'n' : PutB('\n'); break;
    default: PutB(c);
    }
    ctx->p++;
    c = *ctx->p;
  }

  // skip tail double quote
  ctx->p++;
  // null terminate
  PutB('\0');
  ctx->here = align(ctx->here);

  Set(back, ctx->here); // back patch
  PutI(LIT);
  Put(str);
  return 1;
}


void compile_source(Context* ctx) {
  while (*ctx->p != '\0') {
    skip_spaces(ctx);
    if (compile_string(ctx)) continue;
    compile_token(ctx);
  }
}

void compile_all(Context* ctx) {
  ctx->source = reverse_source_inplace(ctx->source);
  while (ctx->source) {
    ctx->p = ctx->source->text;
    ctx->source_name = ctx->source->fname;
    compile_source(ctx);
    Source* src = ctx->source;
    ctx->source = ctx->source->next;
    free_source(src);
  }
}


// Setup and Entrypoint
// =============================================================================

void setup_all(Context* ctx) {
  ArkamVMOptions opts;
  ark_set_default_options(&opts);  
  VM* vm = ark_new_vm(&opts);
  ctx->vm = vm;

  ctx->here  = ARK_ADDR_CODE_BEGIN;
  ctx->start = ctx->here; //TODO: require main

  ctx->token_buf_len = TOKEN_BUF_LEN;
  ctx->token_buf = calloc(sizeof(char), TOKEN_BUF_LEN+1); // for null termination
  if (!ctx->token_buf) die("Can't allocate token buffer");

  ctx->dict = NULL;
  ctx->current = NULL;
  ctx->source = NULL;
  ctx->includes = NULL;
  ctx->search_level = 0;
}

Cell build_entrypoint(Context* ctx, Word* entrypoint) {
  /* setup default exit routine and returns actual entrypoint address
     : main 1 ;    ( valid, returns 1 )
     : main noop ; ( valid, returns 0 )
     compiles: lit 0 &main HALT
         like: : _main 0 main HALT ;
  */
  Cell start = ctx->here;
  PutI(LIT);
  Put(0);
  Put(entrypoint->inst);
  PutI(HALT);
  return start;
}

int should_backpatch(Word* w) {
  return w->type == WordVal;
}

void backpatch(Context* ctx, Word* w) {
  VM* vm = ctx->vm;
  Cell addr = ctx->here;
  Cell link = w->back;
  while (link != 0) {
    Cell next = Get(link);
    Set(link, addr);
    link = next;
  }

  Put(0); // allot
}

void backpatch_all_heap(Context* ctx) {
  // walk all words
  Word* w = ctx->dict;
  while (w) {
    if (should_backpatch(w)) backpatch(ctx, w);
    w = w->child ? w->child : w->next;
  }
}

void save_all(Context* ctx) {
  VM* vm = ctx->vm;
  
  /* ----- check main ----- */
  Word* entrypoint = find_word(ctx, "main");
  if (!entrypoint) die("No main entrypoint");

  /* ----- build entrypoint ----- */
  ctx->start = build_entrypoint(ctx, entrypoint);

  /* ----- prepare heap area ----- */
  ctx->here = align(ctx->here);
  Cell code_size = ctx->here;

  /* ----- heap allotation & backpatching ----- */
  backpatch_all_heap(ctx);

  /* ----- write information ----- */
  /* entrypoint */ Set(ARK_ADDR_START, ctx->start);
  /* here       */ Set(ARK_ADDR_HERE,  ctx->here);

  /* ----- Write out to file ----- */
  if (fwrite(vm->mem, sizeof(Byte), code_size, ctx->image_file) < code_size)
    die("ERROR save_all %s", strerror(errno));
}

void free_all(Context* ctx) {
  fclose(ctx->image_file);  
  ark_free_vm(ctx->vm);
  free_dict(ctx->dict);
}


// CLI
// =============================================================================

void set_default_opts(SolOption* opts) {
  opts->use_corelib = 1;
}

void usage() {
  fprintf(stderr, "%s", USAGE);
  exit(1);
}

int handle_opts(SolOption* opts, Context* ctx, int argc, char* argv[]) {
  // returns start index of rest arguments(optind)
  const char* optstr = "hn";
  
  struct option long_opts[] =
    { { "help",       no_argument, NULL, 'h' },
      { "no-corelib", no_argument, NULL, 'n' },
      { NULL,         0,           0,    0   }
    };
  
  opterr = 0; // disable logging error
  int c;
  int long_index;

  while ((c = getopt_long(argc, argv, optstr, long_opts, &long_index)) != -1) {
    switch (c) {
    case 'h':
      usage();
      exit(0);
    case 'n':
      opts->use_corelib = 0;
      break;
    case '?':
      fprintf(stderr, "Unknown option: %c\n", optopt);
      usage();
      exit(1);
    }
  }

  return optind;
}

int main(int argc, char* argv[]) {
  SolOption opts;
  set_default_opts(&opts);

  Context ctx;

  // setup before handling options and loading sources
  setup_all(&ctx);

  int argi    = handle_opts(&opts, &ctx, argc, argv);
  int restc   = argc - argi;
  int image_i = argc - 1; // last argument

  // require at least one source and one image name
  if (restc < 2) usage();

  // corelib
  if (opts.use_corelib) add_corelib(&ctx);

  for (int i = argi; i < image_i; i++) {
    open_source(&ctx, argv[i]);
  }

  open_image(&ctx, argv[image_i]);

  // main routine
  compile_all(&ctx);
  save_all(&ctx);

  // cleanup
  free_all(&ctx);

  return 0;
}
