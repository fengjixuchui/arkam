#include "arkam.h"
#include <stdarg.h>


#define Public
#define Private static


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
// some defs require declaration of (VM* vm) and use (int code)

typedef ArkamVM VM;
typedef ArkamCode Code;
#define Cells(n)        ((n)*sizeof(Cell))
#define Set(i, v)       (*(Cell*)(vm->mem + (i)) = (v))
#define Get(i)          (*(Cell*)(vm->mem + (i)))
#define Push(v)         (unsafe_push(vm, (v)))
#define Pop()           (unsafe_pop(vm))
#define RPush(v)        (unsafe_rpush(vm, (v)))
#define RPop()          (unsafe_rpop(vm))
#define Tos()           (top_of_stack(vm))
#define Raise(err_name) { vm->err = ARK_ERR_##err_name; return ARK_ERR; }
#define ExpectOK        if (code != ARK_OK) return code;


// Error strings
// =============================================================================

#define ErrStr(name, str) case ARK_ERR_##name: return str

Public char* ark_err_str(int err) {
  switch (err) {
    ErrStr(DS_OVERFLOW,       "data stack overflow");
    ErrStr(DS_UNDERFLOW,      "data stack underflow");
    ErrStr(RS_OVERFLOW,       "return stack overflow");
    ErrStr(RS_UNDERFLOW,      "return stack underflow");
    ErrStr(INVALID_ADDR,      "invalid address");
    ErrStr(INVALID_INST,      "invalid instruction");
    ErrStr(ZERO_DIVISION,     "zero division");
    ErrStr(IO_UNKNOWN_DEV,    "IO: unknown device");
    ErrStr(IO_UNKNOWN_OP,     "IO: unknown operation");
    ErrStr(IO_NOT_REGISTERED, "IO: not registered");
  }
  return NULL;
}



// Memory
// =============================================================================

Public int ark_valid_addr(VM* vm, Cell i) {
  return i > 0 && i < Cells(vm->cells);
}

#define valid_addr ark_valid_addr

Public Code ark_get(VM* vm, Cell i) {
  // safe get, hold vm->result
  if (!valid_addr(vm, i)) Raise(INVALID_ADDR);
  vm->result = Get(i);
  return ARK_OK;
}

Public Code ark_set(VM* vm, Cell i, Cell v) {
  if (!valid_addr(vm, i)) Raise(INVALID_ADDR);
  Set(i, v);
  return ARK_OK;
}


// Data Stack
// =============================================================================

Public int ark_has_ds_items(VM* vm, int n) {
  /* Whether data stack has at least n items?
     sp points 1 cell lower than current top.

     available)
       sp -> | 1 cell can be popped
             | x
       rs -> | Start of return stack

     empty)
       sp -> | BOTTOM OF DS, EMPTY
       rs -> | DO NOT POINT HERE
   */
  return vm->sp + Cells(n) < vm->rs;
}

#define has_ds_items ark_has_ds_items


Public int ark_has_ds_spaces(VM* vm, int n) {
  /* Whether data stack has spaces at least n cells?
     sp points 1 cell lower than current top.

     available)
     sp, ds -> | 1 cell can be pushed
               | x

     full)
         sp -> | DO NOT PUSH TO HERE
         ds -> | x (data stack is full)
  */
  return vm->sp - Cells(n-1) >= vm->ds;
}

#define has_ds_spaces ark_has_ds_spaces


Private Cell top_of_stack(VM* vm) {
  return Get(vm->sp + Cells(1));
}

Private void unsafe_push(VM* vm, Cell v) {
  Set(vm->sp, v);
  vm->sp -= Cells(1);
}

Private Cell unsafe_pop(VM* vm) {
  vm->sp += Cells(1);
  return Get(vm->sp);
}

Public Code ark_push(VM* vm, Cell v) {
  if (!has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
  Push(v);
  return ARK_OK;
}

Public Code ark_pop(VM* vm) {
  // set to vm->result
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
  vm->result = Pop();
  return ARK_OK;
}

Public Code ark_pop_valid_addr(VM* vm) {
  // If tos is valid addr this pops it to vm->result and returns ARK_OK
  // or remains tos for error detection and set error to vm->err.
  if (!valid_addr(vm, Tos())) Raise(INVALID_ADDR);
  
  vm->result = Pop();
  return ARK_OK;
}

#define pop_valid_addr ark_pop_valid_addr


// Return Stack
// =============================================================================

/*
  rp -> | 
        | return address

 <prologue>
 push ip to rstack (return address)

 <epilogue>
 pop ip from rstack
*/

Private int has_rs_items(VM* vm, int n) {
  /* Whether data stack has at least n items?
     rp points 1 cell lower than current top.

     available)
       rp ->   | 1 cell can be popped
               | x
       cells-> ( Out of memory )

     empty)
          rp -> | BOTTOM OF RS, EMPTY
       cells ->   DO NOT POINT HERE
   */
  return vm->rp + Cells(n) < Cells(vm->cells);
}

Private int has_rs_spaces(VM* vm, int n) {
  /* Whether data stack has spaces at least n cells?
     rp points 1 cell lower than current top.

     available)
     rp, rs -> | 1 cell can be pushed
               | x

     full)
         rp -> | DO NOT PUSH TO HERE
         rs -> | x (data stack is full)
  */
  return vm->rp - Cells(n-1) >= vm->rs;
}


Private void unsafe_rpush(VM* vm, Cell v) {
  Set(vm->rp, v);
  vm->rp -= Cells(1);
}

Private Cell unsafe_rpop(VM* vm) {
  vm->rp += Cells(1);
  return Get(vm->rp);
}

Public Code ark_rpush(VM* vm, Cell v) {
  if (!has_rs_spaces(vm, 1)) Raise(RS_OVERFLOW);

  unsafe_rpush(vm, v);
  return ARK_OK;
}

Public Code ark_rpop(VM* vm) {
  // set to vm->result
  if (!has_rs_items(vm, 1)) Raise(RS_UNDERFLOW);

  vm->result = unsafe_rpop(vm);
  return ARK_OK;
}

Private Code prologue(VM* vm) {
  return ark_rpush(vm, vm->ip);
}

Private Code epilogue(VM* vm) {
  Code code;
  code = ark_rpop(vm); ExpectOK; vm->ip = vm->result;
  return ARK_OK;
}


// VM Loop
// =============================================================================

Private Code instHALT(VM* vm) {
  return ARK_HALT;
}

Private Code instLIT(VM* vm) {
  /*       | LIT
     ip -> | x      # push this and
        -> |        # make ip to point here
  */
  int code = ark_get(vm, vm->ip); ExpectOK;
  vm->ip += Cells(1);
  return ark_push(vm, vm->result);
}

Private Code instRET(VM* vm) {
  // ret is exit of traditional forth!
  return epilogue(vm);
}


// Stack Operation

Private Code instDUP(VM* vm) {
  if (!has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
  if (!has_ds_items(vm, 1))  Raise(DS_UNDERFLOW);
  
  Push(Tos());
  return ARK_OK;
}

Private Code instDROP(VM* vm) {
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
  vm->result = Pop();
  return ARK_OK;
}

Private Code instSWAP(VM* vm) {
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  // a b -- b a
  Cell sp = vm->sp;
  Cell ib = sp + Cells(1); // index of b
  Cell ia = sp + Cells(2); // index of a
  Cell tmp = Get(ia);      // tmp = a
  Set(ia, Get(ib));
  Set(ib, tmp);
  return ARK_OK;
}

Private Code instOVER(VM* vm) {
  // a b -- a b a
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  if (!has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
  Cell a = Get(vm->sp + Cells(2));
  Push(a);
  return ARK_OK;
}


// Arithmatics

Private Code instADD(VM* vm) {
  // a b -- a+b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a+b);
  return ARK_OK;
}

Private Code instSUB(VM* vm) {
  // a b -- a-b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a-b);
  return ARK_OK;  
}

Private Code instMUL(VM* vm) {
  // a b -- a*b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a*b);
  return ARK_OK;  
}

Private Code instDMOD(VM* vm) {
  // a b -- a/b a%b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell sp = vm->sp;
  Cell ib = sp + Cells(1);
  Cell ia = sp + Cells(2);
  Cell b  = Get(ib);
  Cell a  = Get(ia);
  if (b == 0) Raise(ZERO_DIVISION); // Do not touch the stack
  Set(ia, a / b);
  Set(ib, a % b);
  return ARK_OK;
}


// Compare

Private Code instEQ(VM* vm) {
  // a b -- a=b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a == b ? -1 : 0);
  return ARK_OK;
}

Private Code instNEQ(VM* vm) {
  // a b -- a!=b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a != b ? -1 : 0);
  return ARK_OK;
}

Private Code instGT(VM* vm) {
  // a b -- a>b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a > b ? -1 : 0);
  return ARK_OK;
}

Private Code instLT(VM* vm) {
  // a b -- a<b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
  Cell b = Pop();
  Cell a = Pop();
  Push(a < b ? -1 : 0);
  return ARK_OK;
}


// Control flow

Private Code instJMP(VM* vm) {
  /* Jump to addr (ip will be changed)
          | JMP
    ip -> | addr
  */
  Code code = ark_get(vm, vm->ip); ExpectOK;
  Cell addr = vm->result;
  if (!valid_addr(vm, addr)) Raise(INVALID_ADDR);
  vm->ip = addr;
  return ARK_OK;
}

Private Code instZJMP(VM* vm) {
  // same with instJMP if TOS is zero
  // or skip addr
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);

  // Tos is not a zero, skip
  if (Pop() != 0) {
    Cell next = vm->ip + Cells(1);
    if (!valid_addr(vm, next)) Raise(INVALID_ADDR);
    vm->ip = next;
    return ARK_OK;
  }
  
  Code code = ark_get(vm, vm->ip); ExpectOK;
  Cell addr = vm->result;
  if (!valid_addr(vm, addr)) Raise(INVALID_ADDR);
  vm->ip = addr;
  return ARK_OK;
}


// Memory

Private Code instGET(VM* vm) {
  // & -- v
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);

  Code code = pop_valid_addr(vm); ExpectOK;
  Cell addr = vm->result;

  Push(Get(addr));
  return ARK_OK;
}

Private Code instSET(VM* vm) {
  // v & --
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Code code = pop_valid_addr(vm); ExpectOK;
  Cell addr = vm->result;

  Cell v = Pop();
  Set(addr, v);

  return ARK_OK;
}

Private Code instBGET(VM* vm) {
  // & -- v
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);

  Code code = pop_valid_addr(vm); ExpectOK;
  Cell addr = vm->result;

  Byte v = vm->mem[addr];
  Push(v);

  return ARK_OK;
}

Private Code instBSET(VM* vm) {
  // v & --
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Code code = pop_valid_addr(vm); ExpectOK;
  Cell addr = vm->result;

  Byte v = Pop();
  vm->mem[addr] = v;
  
  return ARK_OK;
}


// Bitwise

Private Code instAND(VM* vm) {
  // a b -- a&b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell b = Pop();
  Cell a = Pop();
  Push(a & b);
  return ARK_OK;
}

Private Code instOR(VM* vm) {
  // a b -- a|b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell b = Pop();
  Cell a = Pop();
  Push(a | b);
  return ARK_OK;
}

Private Code instNOT(VM* vm) {
  // a -- ~a
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);

  Cell a = Pop();
  Push(~a);
  return ARK_OK;
}

Private Code instXOR(VM* vm) {
  // a b -- a^b
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell b = Pop();
  Cell a = Pop();
  Push(a ^ b);
  return ARK_OK;
}

Private Code instLSHIFT(VM* vm) {
  // logical shift
  // a b -- a<<b (b > 0)
  // a b -- a>>b (b < 0)
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell  b = Pop();
  UCell a = Pop(); // for logical shift
  Push(b > 0 ? a << b : a >> (b * -1));
  return ARK_OK;
}

Private Code instASHIFT(VM* vm) {
  // arithmetic shift
  // a b -- a<<b (b > 0)
  // a b -- a>>b (b < 0)
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell b = Pop();
  Cell a = Pop();
  Push(b > 0 ? a << b : a >> (b * -1));
  return ARK_OK;
}


// I/O

#define IO_READY_QUERY -1

Private Code instIO(VM* vm) {
  // op device -- ...
  if (!has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);

  Cell dev = Pop();
  Cell op  = Pop();

  // handler
  if (dev >= 0 && dev < ARK_DEVICES_COUNT) {
    ArkamDeviceHandler handler = vm->io_handlers[dev];

    if (op == IO_READY_QUERY) {
      Push(handler == NULL ? 0 : -1); return ARK_OK;
    }
    
    if (handler == NULL) Raise(IO_NOT_REGISTERED);
    return handler(vm, op);
  }

  // unknown device
  if (op == 0) {
    Push(0); // not available
    return ARK_OK;
  }
  Raise(IO_UNKNOWN_DEV);
}


// Sys Handler

Private Code handleSYS(VM* vm, Cell op) {
  switch (op) {

  case 0:
    /* Memory bytes */
    Push(vm->cells * sizeof(Cell));
    return ARK_OK;

  case 2:
    /* Deta stack size(cells) */
    Push(vm->ds_size);
    return ARK_OK;
    
  case 3:
    /* Data stack address */
    Push(vm->ds);
    return ARK_OK;

  case 4:
    /* Return stack size(cells) */
    Push(vm->rs_size);
    return ARK_OK;
    
  case 5:
    /* Return stack address */
    Push(vm->rs);
    return ARK_OK;

  case 6:
    /* Cell size (bytes) */
    Push(sizeof(Cell));
    return ARK_OK;

  case 7:
    /* Maximum integer(cell) */
    Push(ARK_MAX_INT);
    return ARK_OK;

  case 8:
    /* Minimum integer(cell) */
    Push(ARK_MIN_INT);
    return ARK_OK;
    
  default: Raise(IO_UNKNOWN_OP);
  }
}


// Return stack

Private Code instRPUSH(VM* vm) {
  // v -- r:v
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
  if (!has_rs_spaces(vm, 1)) Raise(RS_OVERFLOW);
  RPush(Pop());
  return ARK_OK;
}

Private Code instRPOP(VM* vm) {
  // r:v -- v
  if (!has_ds_spaces(vm, 1))   Raise(DS_OVERFLOW);
  if (!has_rs_items(vm, 1))    Raise(RS_UNDERFLOW);
  Push(RPop());
  return ARK_OK;
}

Private Code instRDROP(VM* vm) {
  // r:v --
  if (!has_rs_items(vm, 1))    Raise(RS_UNDERFLOW);
  RPop();
  return ARK_OK;
}


// Registers

Private Code instGETSP(VM* vm) {
  // -- v
  if (!has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
  Push(vm->sp);
  return ARK_OK;
}

Private Code instSETSP(VM* vm) {
  // v --
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
  Code code = pop_valid_addr(vm); ExpectOK;
  vm->sp = vm->result;
  return ARK_OK;
}

Private Code instGETRP(VM* vm) {
  // -- v
  if (!has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
  Push(vm->rp);
  return ARK_OK;
}

Private Code instSETRP(VM* vm) {
  // v --
  if (!has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
  Code code = pop_valid_addr(vm); ExpectOK;
  vm->rp = vm->result;
  return ARK_OK;  
}


// inst table

typedef Code(*InstHandler)(VM* vm);

Private InstHandler InstTable[ARK_INSTRUCTION_COUNT] =
  { NULL, // noop
    instHALT,
    instLIT,
    instRET,
    // Stack
    instDUP,
    instDROP,
    instSWAP,
    instOVER,
    // Arithmetics
    instADD,
    instSUB,
    instMUL,
    instDMOD,
    // Compare
    instEQ,
    instNEQ,
    instGT,
    instLT,
    // Control flow
    instJMP,
    instZJMP,
    // Memory
    instGET,
    instSET,
    instBGET,
    instBSET,
    // Bitwise
    instAND,
    instOR,
    instNOT,
    instXOR,
    instLSHIFT,
    instASHIFT,
    // Peripheral
    instIO,
    // Return stack
    instRPUSH,
    instRPOP,
    instRDROP,
    // Registers
    instGETSP,
    instSETSP,
    instGETRP,
    instSETRP,
  };


// step and run

Public Code ark_step(VM* vm) {
  Code code = ark_get(vm, vm->ip); ExpectOK;
  Cell inst = vm->result;

  if (!valid_addr(vm, inst)) Raise(INVALID_INST);
  if (inst == 0) Raise(INVALID_INST);
    
  vm->ip += Cells(1);

  // use jump table for primitives
  if (inst & 0x01) {
    inst = inst >> 1;
    if (inst == ARK_INST_NOOP) return ARK_OK;
    return InstTable[inst](vm);
  }
  
  /* Step into a word
     ip -> | inst (*1)
           | next inst (return to here)
           | ...
      (*1) | word code ...
  */

  code = prologue(vm); ExpectOK;
  vm->ip = inst;
  return ARK_OK;
}

Public Code ark_run(VM* vm) {
  Code code = ARK_OK;
  while (code == ARK_OK) {
    code = ark_step(vm);
  }
  return code;
}



// VM setup
// =============================================================================

Public void ark_set_default_options(ArkamVMOptions* opts) {
  opts->memory_cells = ARK_DEFAULT_MEM_CELLS;
  opts->dstack_cells = ARK_DEFAULT_DS_CELLS;
  opts->rstack_cells = ARK_DEFAULT_RS_CELLS;
}

Public VM* ark_new_vm(ArkamVMOptions* opts) {
  VM* vm = calloc(sizeof(VM), 1);

  Cell msize  = opts->memory_cells;
  Cell dsize  = opts->dstack_cells;
  Cell rsize  = opts->rstack_cells;
  Cell entire = msize + dsize + rsize;
  Cell bytes  = entire * sizeof(Cell);
  vm->cells = entire;
  vm->ds_size = dsize;
  vm->rs_size = rsize;
  vm->mem = calloc(sizeof(Byte), bytes);

  if (!vm->mem) return NULL;

  // calculate & store addresses
  // ex. mem: 128, dsize: 64, rsize: 64, entire: 256
  vm->rs = Cells(entire - rsize);   // rs: 192
  vm->rp = Cells(entire - 1);       // rp: 255
  vm->ds = vm->rs - Cells(dsize);   // ds: 128
  vm->sp = vm->rs - Cells(1);       // sp: 191

  vm->ip = 0;

  vm->io_handlers[ARK_DEVICE_SYS] = handleSYS;

  return vm;
}

Public void ark_free_vm(VM* vm) {
  free(vm->mem);
  free(vm);
}
