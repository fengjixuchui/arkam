#include "arkam.h"
#include <assert.h>
#include <stdarg.h>
#include "shorthands.h"


// Debug print
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

void put(VM* vm, Cell* here, Cell v) {
  Set(*here, v);
  *here = *here + Cells(1);
}

#define Put(here, v) (put(vm, (&here), (v)))

#define PutI(here, inst) (Put(here, (ARK_INST_##inst << 1) | 0x01))

void run(VM* vm, Cell start, ArkamCode expect) {
  if (start & 0x01) DIE("Unaligned start point: %d");
  vm->ip = start;
  Code code = ark_run(vm);
  assert(code == expect);
}

#define Run(start, expect) (run(vm, start, expect))


// VM Basics
// =============================================================================

void test_memory_access(Opts* opts) {
  ArkamVM* vm = ark_new_vm(opts);
  // valid access
  
  assert(ark_set(vm, 0x04, 123) == ARK_OK);
  assert(ark_get(vm, 0x04) == ARK_OK);
  assert(vm->result == 123);

  assert(ark_set(vm, Cells(11), 123) == ARK_OK);
  assert(ark_get(vm, Cells(11)) == ARK_OK);
  assert(vm->result == 123);  

  // out of range
  assert(ark_set(vm, -1, 123) == ARK_ERR);
  assert(ark_get(vm, -1)      == ARK_ERR);
  assert(ark_set(vm, Cells(12), 123) == ARK_ERR);
  assert(ark_get(vm, Cells(12))      == ARK_ERR);

  // DO NOT ACCESS 0x00
  assert(ark_set(vm, 0, 123) == ARK_ERR);
  assert(ark_get(vm, 0)      == ARK_ERR);

  ark_free_vm(vm);
}


void test_data_stack(Opts* opts) {
  Cell dcells = 16;
  opts->dstack_cells = dcells;
  
  ArkamVM* vm = ark_new_vm(opts);
  int code = ARK_OK;

  // Push to fill
  for (int i = 0; i < dcells; i++) {
    code = ark_push(vm, 255+i); // check byte/cell conversion
    assert(code == ARK_OK);
  }

  // stack overflow
  code = ark_push(vm, 0);
  assert(code == ARK_ERR);
  assert(vm->err == ARK_ERR_DS_OVERFLOW);

  // Pop all
  for (int i = dcells; i > 0; i--) {
    code = ark_pop(vm);
    assert(code == ARK_OK);
    assert(vm->result == (255+i) - 1); // again, check byte/cell conversion
  }

  // stack underflow
  code = ark_pop(vm);
  assert(code == ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);

  ark_free_vm(vm);
}


void test_return_stack(Opts* opts) {
  Cell rcells = 16;
  opts->rstack_cells = rcells;
  
  ArkamVM* vm = ark_new_vm(opts);
  int code = ARK_OK;

  // Push to fill
  for (int i = 0; i < rcells; i++) {
    code = ark_rpush(vm, 255+i); // check byte/cell conversion
    assert(code == ARK_OK);
  }

  // stack overflow
  code = ark_rpush(vm, 0);
  assert(code == ARK_ERR);
  assert(vm->err == ARK_ERR_RS_OVERFLOW);

  // Pop all
  for (int i = rcells; i > 0; i--) {
    code = ark_rpop(vm);
    assert(code == ARK_OK);
    assert(vm->result == (255+i) - 1); // again, check byte/cell conversion
  }

  // stack underflow
  code = ark_rpop(vm);
  assert(code == ARK_ERR);
  assert(vm->err == ARK_ERR_RS_UNDERFLOW);

  ark_free_vm(vm);
}

void test_run_call_word(VM* vm) {
  // stack: 40 2
  Push(40);
  Push(2);

  // : foo + ret ;
  Cell foo  = ARK_ADDR_CODE_BEGIN;
  Cell here = foo;
  PutI(here, ADD);
  PutI(here, RET);
 
  // foo halt
  Cell start = here;
  Put(here, foo);
  PutI(here, HALT);

  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_invalid_inst(VM* vm) {
  // 0 0 0 ... 0
  Cell start = ARK_ADDR_CODE_BEGIN;
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_INVALID_INST);

  // -1
  start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  Put(here, -1);
  Put(here, ARK_HALT);  
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_INVALID_INST);

  // TOO_LARGE
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  Put(here, Cells(vm->cells));
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_INVALID_INST);
}

void test_run_lit(VM* vm) {
  // lit 42 halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, HALT);

  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_dup(VM* vm) {
  // lit 21 dup add halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 21);
  PutI(here, DUP);
  PutI(here, ADD);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // dup halt (failed for underflow)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, DUP);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);

  // lit 42 dup halt (failed for overflow)
  vm->sp = vm->ds; // 1 cells can be pushed
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 21);
  PutI(here, DUP);
  PutI(here, ADD);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_OVERFLOW);
  assert(Pop() == 21); // lit 42
}

void test_run_drop(VM* vm) {
  // lit 42 lit 43 drop halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 43);
  PutI(here, DROP);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // drop halt (failed for underflow)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, DROP);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);
}

void test_run_swap(VM* vm) {
  // lit 42 lit 43 swap halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 43);
  PutI(here, SWAP);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
  assert(Pop() == 43);

  // lit 42 swap halt (failed for underflow)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, SWAP);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);
  assert(Pop() == 42);

  // swap halt (failed for underflow)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, SWAP);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);
}

void test_run_over(VM* vm) {
  // lit 42 lit 0 over halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, OVER);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
  Pop(); Pop(); // clean up

  // lit 42 over halt (failed for underflow)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, OVER);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_UNDERFLOW);

  // lit 42 lit 0 over halt (failed for overflow)
  vm->sp = vm->ds + Cells(1); // 2 cells can be pushed
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, OVER);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_DS_OVERFLOW);
  assert(Pop() == 0); // lit 0
}


void test_run_sub(VM* vm) {
  // 43 1 sub halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 43);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, SUB);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_mul(VM* vm) {
  // 21 2 mul halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 21);
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, MUL);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_dmod(VM* vm) {
  // 7 3 dmod halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 7);
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, DMOD);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 1);
  assert(Pop() == 2);

  // 42 0 dmod halt (failed for zero division)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, DMOD);
  PutI(here, HALT);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_ZERO_DIVISION);
  // keep stack
  assert(Pop() == 0);
  assert(Pop() == 42);  
}

void test_run_eq(VM* vm) {
  // 1 1 eq halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, EQ);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);

  // 1 2 eq halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, EQ);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);
}

void test_run_neq(VM* vm) {
  // 1 1 neq halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, NEQ);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);

  // 1 2 neq halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, NEQ);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);
}

void test_run_gt(VM* vm) {
  // 2 1 gt halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, GT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);

  // 1 1 gt halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, GT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);

  // 1 2 gt halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, GT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);  
}

void test_run_lt(VM* vm) {
  // 2 1 lt halt
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);

  // 1 1 lt halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);

  // 1 2 lt halt
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LIT);
  Put(here, 2);
  PutI(here, LT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);  
}


void test_run_jmp(VM* vm) {
  // lit 42 halt jmp (first), start from jmp
  Cell word = ARK_ADDR_CODE_BEGIN;
  Cell here = word;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, HALT);
  Cell start = here;
  PutI(here, JMP);
  Put(here, word);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // jmp -1 (failed for invalid address)
  start = ARK_ADDR_CODE_BEGIN;
  here = word;
  PutI(here, JMP);
  Put(here, -1);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_INVALID_ADDR);
}

void test_run_zjmp(VM* vm) {
  // lit 42 halt (START) lit 0 zjmp (first), start from START
  Cell word = ARK_ADDR_CODE_BEGIN;
  Cell here = word;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, HALT);
  Cell start = here;
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, ZJMP);
  Put(here, word);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // lit -1 zjmp -1 lit 42 halt, skip jmp
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, -1);
  PutI(here, ZJMP);
  Put(here, -1);
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_getset(VM* vm) {
  // (HERE) 0 (START) lit 42 lit &HERE set lit &HERE get halt
  Cell var = ARK_ADDR_CODE_BEGIN;
  Cell here = var;
  Put(here, 0);
  Cell start = here;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, var);
  PutI(here, SET);
  PutI(here, LIT);
  Put(here, var);
  PutI(here, GET);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_bgetset(VM* vm) {
  // (HERE) -1 (START) lit 42 lit &HERE bset lit &HERE bget lit &HERE+1 bget add halt
  // set to 42 255(FF) 255(FF) 255(FF)
  Cell var = ARK_ADDR_CODE_BEGIN;
  Cell here = var;
  Put(here, -1);
  Cell start = here;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, var);
  PutI(here, BSET);
  PutI(here, LIT);
  Put(here, var);
  PutI(here, BGET);
  PutI(here, LIT);
  Put(here, var+1);
  PutI(here, BGET);
  PutI(here, ADD);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42 + 255);
}

void test_run_bitwise_and(VM* vm) {
  // lit 3(0b011) lit 6(0b110) and halt => 2(0b010)
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, LIT);
  Put(here, 6);
  PutI(here, AND);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 2);
}

void test_run_bitwise_or(VM* vm) {
  // lit 3(0b011) lit 4(0b100) or halt => 7(0b111)
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, LIT);
  Put(here, 4);
  PutI(here, OR);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 7);
}

void test_run_bitwise_not(VM* vm) {
  // lit 0 not halt => -1
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, NOT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);

  // lit -1 not halt => 0
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, NOT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -1);
}

void test_run_bitwise_xor(VM* vm) {
  // lit 3(0b011) lit 5(0b101) xor halt => 6(0b110)
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, LIT);
  Put(here, 5);
  PutI(here, XOR);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 6);  
}

void test_run_bitwise_lshift(VM* vm) {
  // logical shift
  // lit 3 lit 1 lshift halt => 6
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, LSHIFT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 6);

  // lit -3 lit -1 lshift halt => (logical right shifted)
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, -3);
  PutI(here, LIT);
  Put(here, -1);
  PutI(here, LSHIFT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  UCell x = -3;
  assert(Pop() == x >> 1);      
}

void test_run_bitwise_ashift(VM* vm) {
  // arithmetic shift
  // lit 3 lit 1 lshift halt => 6
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 3);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, ASHIFT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 6);

  // lit -3 lit -1 lshift halt => -2
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, LIT);
  Put(here, -3);
  PutI(here, LIT);
  Put(here, -1);
  PutI(here, ASHIFT);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == -2);        
}


// IO

void test_run_io(VM* vm) {
  // lit -1 lit 1 io halt => -1 (stdio is not ready)
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, -1);
  PutI(here, LIT);
  Put(here, 1);
  PutI(here, IO);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 0);
}


// Return stack

void test_run_return_stack(VM* vm) {
  // lit 42 rpush rpop halt => 42
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, RPUSH);
  PutI(here, RPOP);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // rpop => failed for rs underflow
  start = ARK_ADDR_CODE_BEGIN;
  here = start;
  PutI(here, RPOP);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_RS_UNDERFLOW);

  // lit 42 rpush lit 0 rpush rdrop rpop halt => 42
  here = start = ARK_ADDR_CODE_BEGIN;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, RPUSH);
  PutI(here, LIT);
  Put(here, 0);
  PutI(here, RPUSH);
  PutI(here, RDROP);
  PutI(here, RPOP);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);

  // rdrop => failed for rs underflow
  here = start = ARK_ADDR_CODE_BEGIN;
  PutI(here, RDROP);
  Run(start, ARK_ERR);
  assert(vm->err == ARK_ERR_RS_UNDERFLOW);
}

void test_run_sp(VM* vm) {
  // lit 42 lit 43 sp lit 4 add sp! halt => 42
  // in forth: `42 43 sp 4 + sp! halt` same as `42 43 drop halt`
  Cell start = ARK_ADDR_CODE_BEGIN;
  Cell here = start;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, LIT);
  Put(here, 43);
  PutI(here, GETSP);
  PutI(here, LIT);
  Put(here, 4);
  PutI(here, ADD);
  PutI(here, SETSP);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}

void test_run_rp(VM* vm) {
  // (WORD) lit 42 rpush lit -1 rpush rp lit 4 add rp! rpop ret
  // (START) WORD halt => 42
  Cell word = ARK_ADDR_CODE_BEGIN;
  Cell here = word;
  PutI(here, LIT);
  Put(here, 42);
  PutI(here, RPUSH);
  PutI(here, LIT);
  Put(here, -1);
  PutI(here, RPUSH);
  PutI(here, GETRP);
  PutI(here, LIT);
  Put(here, 4);
  PutI(here, ADD);
  PutI(here, SETRP);
  PutI(here, RPOP);
  PutI(here, RET);
  Cell start = here;
  Put(here, word);
  PutI(here, HALT);
  Run(start, ARK_HALT);
  assert(Pop() == 42);
}


#define do_test(name) {                                                    \
  printf("test %30s ...", #name);                                          \
  Opts opts = { .memory_cells = 4, .dstack_cells = 4, .rstack_cells = 4 }; \
  test_##name(&opts);                                                      \
  printf("done\n");                                                        \
  }
  
#define do_run_test(name) {                                                \
  printf("test %30s ...", "run " #name);                                   \
  Cell inst_count = ARK_INSTRUCTION_COUNT;                                 \
  Opts opts = { .memory_cells = 4, .dstack_cells = 4, .rstack_cells = 4 }; \
  opts.memory_cells = inst_count * 4;                                      \
  ArkamVM* vm = ark_new_vm(&opts);                                         \
  test_run_##name(vm);                                                     \
  ark_free_vm(vm);                                                         \
  printf("done\n");                                                        \
  }


int main(int argc, char* argv[]) {
  do_test(memory_access);
  do_test(data_stack);
  do_test(return_stack);
  
  // ----- Run test -----
  do_run_test(call_word);
  do_run_test(invalid_inst);
  do_run_test(lit);
  // Stack
  do_run_test(dup);
  do_run_test(drop);
  do_run_test(swap);
  do_run_test(over);
  // Arithmetics
  do_run_test(sub);
  do_run_test(mul);
  do_run_test(dmod);
  // Compare
  do_run_test(eq);
  do_run_test(neq);
  do_run_test(gt);
  do_run_test(lt);
  // Control flow
  do_run_test(jmp);
  do_run_test(zjmp);
  // Memory
  do_run_test(getset);
  do_run_test(bgetset);
  // Bitwise
  do_run_test(bitwise_and);
  do_run_test(bitwise_or);
  do_run_test(bitwise_not);
  do_run_test(bitwise_xor);
  do_run_test(bitwise_lshift);
  do_run_test(bitwise_ashift);
  // Peripheral
  do_run_test(io);
  // Return Stack
  do_run_test(return_stack);
  // Registers
  do_run_test(sp);
  do_run_test(rp);
  return 0;
}
