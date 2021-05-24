#if !defined(__ARKAM_SHORTHANDS__)
#define __ARKAM_SHORTHANDS__


#include "arkam.h"
#include <assert.h>


// ===== Shorthands for test and sol =====

#define Cells(n) ((n)*sizeof(Cell))
#define ExpectOK  assert(code == ARK_OK)
#define Get(i)    (force_get(vm, (i)))
#define Set(i, v) (force_set(vm, (i), (v)))
#define Pop()     (force_pop(vm))
#define Push(v)   (force_push(vm, (v)))
#define Raise(err_name) { vm->err = ARK_ERR_##err_name; return ARK_ERR; }
typedef ArkamVM        VM;
typedef ArkamVMOptions Opts;
typedef ArkamCode      Code;


Cell force_get(VM* vm, Cell i);
void force_set(VM* vm, Cell i, Cell v);
Cell force_pop(VM* vm);
void force_push(VM* vm, Cell v);


#define PopValid(v) {                \
  Code c = ark_pop_valid_addr(vm);   \
  if (c != ARK_OK) return ARK_ERR;   \
  *(v) = vm->result;                 \
  }


#endif
